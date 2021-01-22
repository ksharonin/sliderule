/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#ifdef __aws__
#include "S3Lib.h"
#endif

#include "H5Lite.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * DEFINES
 ******************************************************************************/


/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef union {
    struct {
        uint32_t depth;
        uint32_t max;
    } curr;
    uint64_t data;
} rdepth_t;

/******************************************************************************
 * FILE DATA
 ******************************************************************************/


/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * get_field
 *----------------------------------------------------------------------------*/
uint64_t get_field(uint8_t* buffer, int buffer_size, int* field_offset, int field_size)
{
    if(*field_offset + field_size >= buffer_size)
    {
        return 0;
    }

    switch(field_size)
    {
        case 8:     
        {
            uint64_t val = *(uint64_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapll(val);
            #else
                return val; 
            #endif
        }

        case 4:     
        {
            uint32_t val = *(uint32_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapl(val);
            #else
                return val; 
            #endif
        }

        case 2:
        {
            uint16_t val = *(uint16_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swaps(val);
            #else
                return val; 
            #endif
        }

        default:
        {
            return 0;
        }
    }
}

/******************************************************************************
 * H5 FILE BUFFER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::H5FileBuffer (const char* filename, bool error_checking)
{
    /* Initialize */
    buffSize = 0;
    currBuffPosition = 0;
    currBuffOffset = 0;
    offsetSize = 0;
    lengthSize = 0;
    groupLeafNodeK = 0;
    groupInternalNodeK = 0;
    rootGroupOffset = 0;
    
    /* Open File */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        mlog(CRITICAL, "Failed to open filename: %s", filename);
        throw std::runtime_error("failed to open file");
    }

    /* Read and Verify Superblock Info */
    if(error_checking)
    {
        uint64_t signature = readField(8);
        if(signature != H5_SIGNATURE_LE)
        {
            mlog(CRITICAL, "Invalid h5 file signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid signature");
        }

        uint64_t superblock_version = readField(1);
        if(superblock_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file superblock version: %d\n", (int)superblock_version);
            throw std::runtime_error("invalid superblock version");
        }

        uint64_t freespace_version = readField(1);
        if(freespace_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file free space version: %d\n", (int)freespace_version);
            throw std::runtime_error("invalid free space version");
        }

        uint64_t roottable_version = readField(1);
        if(roottable_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file root table version: %d\n", (int)roottable_version);
            throw std::runtime_error("invalid root table version");
        }

        uint64_t headermsg_version = readField(1);
        if(headermsg_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file header message version: %d\n", (int)headermsg_version);
            throw std::runtime_error("invalid header message version");
        }
    }

    /* Read Size and Root Info */
    offsetSize          = readField(1, 13);
    lengthSize          = readField(1, 14);
    groupLeafNodeK      = readField(2, 16);
    groupInternalNodeK  = readField(2, 18);
    rootGroupOffset     = readField(USE_OFFSET_SIZE, 64);

    /* Read Root Group */
    readObjectHeader(rootGroupOffset);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::~H5FileBuffer (void)
{
    fclose(fp);
}

/*----------------------------------------------------------------------------
 * displayFileInfo
 *----------------------------------------------------------------------------*/
void H5FileBuffer::displayFileInfo (void)
{
    mlog(RAW, "\n----------------\n");
    mlog(RAW, "File Information\n");
    mlog(RAW, "----------------\n");
    mlog(RAW, "Size of Offsets:             %lu\n",     (unsigned long)offsetSize);
    mlog(RAW, "Size of Lengths:             %lu\n",     (unsigned long)lengthSize);
    mlog(RAW, "Group Leaf Node K:           %lu\n",     (unsigned long)groupLeafNodeK);
    mlog(RAW, "Group Internal Node K:       %lu\n",     (unsigned long)groupInternalNodeK);
    mlog(RAW, "Root Object Header Address:  0x%lX\n",   (long unsigned)rootGroupOffset);
}

/*----------------------------------------------------------------------------
 * getCurrPos
 *----------------------------------------------------------------------------*/
int64_t H5FileBuffer::getCurrPos (void)
{
    return currBuffPosition + currBuffOffset;
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readField (int size, int64_t pos)
{
    /* Set Field Size */
    int field_size;
    if      (size == USE_OFFSET_SIZE)   field_size = offsetSize;
    else if (size == USE_LENGTH_SIZE)   field_size = lengthSize;
    else if (size > 0)                  field_size = size;
    else throw std::runtime_error("size of field cannot be negative");

    /* Set Field Position */
    int64_t field_position;
    if      (pos == USE_CURRENT_POSITION)   field_position = currBuffPosition + currBuffOffset;
    else if (pos > 0)                       field_position = pos;
    else throw std::runtime_error("position of field cannot be negative");

    /* Check if Different Data Needs to be Buffered */
    if((field_position < currBuffPosition) || ((field_position + field_size) > (currBuffPosition + buffSize)))
    {
        if(fseek(fp, field_position, SEEK_SET) == 0)
        {
            buffSize = fread(buffer, 1, READ_BUFSIZE, fp);
            currBuffOffset = 0;
            currBuffPosition = field_position;
        }
        else
        {
            throw std::runtime_error("failed to go to field position");
        }
    }

    /* Set Buffer Position */
    currBuffOffset = field_position - currBuffPosition;

    /*  Read Field Value */
    uint64_t value;
    switch(field_size)
    {
        case 8:     
        {
            value = *(uint64_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swapll(value);
            #endif
            break;
        }

        case 4:     
        {
            value = *(uint32_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *(uint16_t*)&buffer[currBuffOffset];
            #ifdef __BE__
                value = LocalLib::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = buffer[currBuffOffset];
            break;
        }

        default:
        {
            assert(0);
            throw std::runtime_error("invalid field size");
        }
    }

    /* Increment Current Buffer Position */
    currBuffOffset += field_size;

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * readData
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readData (uint8_t* data, int size, int pos)
{
    assert(data);
    assert(size > 0);
    assert(pos > 0);

    /* Set Data Position */
    if(fseek(fp, pos, SEEK_SET) == 0)
    {
        currBuffOffset = 0;
        currBuffPosition = pos;
    }
    else
    {
        throw std::runtime_error("failed to go to data position");
    }

    /* Read Data */
    int data_left_to_read = size;
    while(data_left_to_read > 0)
    {
        int data_already_read = size - data_left_to_read;
        int data_to_read = MIN(READ_BUFSIZE, data_left_to_read);
        buffSize = fread(&data[data_already_read], 1, data_to_read, fp);
        data_left_to_read -= buffSize;
        currBuffOffset += buffSize;
    }
}

/*----------------------------------------------------------------------------
 * readObjectHeader
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readObjectHeader (int pos, bool error_checking, bool verbose)
{
    static const int OBJ_HDR_FLAG_SIZE_OF_CHUNK_0_MASK      = 0x03;
    static const int OBJ_HDR_FLAG_ATTR_CREATION_TRACK_BIT   = 0x04;
    static const int OBJ_HDR_FLAG_ATTR_CREATION_INDEX_BIT   = 0x08;
    static const int OBJ_HDR_FLAG_STORE_CHANGE_PHASE_BIT    = 0x10;
    static const int OBJ_HDR_FLAG_FILE_STATS_BIT            = 0x20;

    /* Read Object Header */
    uint64_t signature = readField(4, pos);
    uint64_t version = readField(1);
    if(error_checking)
    {
        if(signature != H5_HDR_SIGNATURE_LE)
        {
            mlog(CRITICAL, "invalid header signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("invalid header signature");
        }

        if(version != 2)
        {
            mlog(CRITICAL, "invalid header version: %d\n", (int)version);
            throw std::runtime_error("invalid header version");
        }
    }

    /* Read Option Time Fields */
    uint8_t obj_hdr_flags = (uint8_t)readField(1);
    if(obj_hdr_flags & OBJ_HDR_FLAG_FILE_STATS_BIT)
    {
        uint64_t access_time         = readField(4);
        uint64_t modification_time   = readField(4);
        uint64_t change_time         = readField(4);
        uint64_t birth_time          = readField(4);

        if(verbose)
        {
            TimeLib::gmt_time_t access_gmt = TimeLib::gettime(access_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Access Time:         %d:%d:%d:%d:%d\n", access_gmt.year, access_gmt.day, access_gmt.hour, access_gmt.minute, access_gmt.second);

            TimeLib::gmt_time_t modification_gmt = TimeLib::gettime(modification_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Modification Time:   %d:%d:%d:%d:%d\n", modification_gmt.year, modification_gmt.day, modification_gmt.hour, modification_gmt.minute, modification_gmt.second);

            TimeLib::gmt_time_t change_gmt = TimeLib::gettime(change_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Change Time:         %d:%d:%d:%d:%d\n", change_gmt.year, change_gmt.day, change_gmt.hour, change_gmt.minute, change_gmt.second);

            TimeLib::gmt_time_t birth_gmt = TimeLib::gettime(birth_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Birth Time:          %d:%d:%d:%d:%d\n", birth_gmt.year, birth_gmt.day, birth_gmt.hour, birth_gmt.minute, birth_gmt.second);
        }
    }

    /* Optional Phase Attributes */
    if(obj_hdr_flags & OBJ_HDR_FLAG_STORE_CHANGE_PHASE_BIT)
    {
        uint64_t max_compact_attr = readField(2); (void)max_compact_attr;
        uint64_t max_dense_attr = readField(2); (void)max_dense_attr;
    }

    /* Read Header Messages */
    uint64_t size_of_chunk0 = readField(1 << (obj_hdr_flags & OBJ_HDR_FLAG_SIZE_OF_CHUNK_0_MASK));
    uint64_t chunk0_data_read = 0;
    int msg_num = 0;
    while(chunk0_data_read < size_of_chunk0)
    {
        uint8_t     hdr_msg_type    = (uint8_t)readField(1);
        uint16_t    hdr_msg_size    = (uint16_t)readField(2);
        uint8_t     hdr_msg_flags   = (uint8_t)readField(1);
        chunk0_data_read += 4;

        uint64_t hdr_msg_order = 0;
        if(obj_hdr_flags & OBJ_HDR_FLAG_ATTR_CREATION_TRACK_BIT)
        {
            hdr_msg_order = (uint8_t)readField(2);
            chunk0_data_read += 2;
        }

        /* Process Each Message Type */
        switch(hdr_msg_type)
        {
            case 2:
            {
                readObjectLink(getCurrPos());
                break;
            }

            default:
            {
                uint8_t hdr_msg_data[0x10000];
                readData(hdr_msg_data, hdr_msg_size, getCurrPos());
                break;
            }
        }

        chunk0_data_read += hdr_msg_size;

        if(verbose)
        {
            mlog(RAW, "Message[%d]:          type=%d, size=%d, flags=%x, order=%d, end=%x\n", msg_num++, (int)hdr_msg_type, (int)hdr_msg_size, (int)hdr_msg_flags, (int)hdr_msg_order, chunk0_data_read);
        }
    }

    uint64_t check_sum = readField(4);
    if(error_checking)
    {
        (void)check_sum;
    }
}

/*----------------------------------------------------------------------------
 * readObjectLink
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readObjectLink (int pos, bool error_checking, bool verbose)
{
    uint64_t version = readField(1, pos);
    uint64_t flags = readField(1);

    if(error_checking)
    {
        if(version != 0)
        {
            mlog(CRITICAL, "invalid link version: %d\n", version);
            throw std::runtime_error("invalid link version");
        }
    }

    if(flags & 0x1)
    {
        uint64_t max_create_index = readField(8); (void)max_create_index;
        if(verbose)
        {
            mlog(RAW, "Maximum Creation Index:  %lu\n", (unsigned long)max_create_index);
        }
    }

    uint64_t heap_address = readField();
    uint64_t name_index = readField();
    if(verbose)
    {
        mlog(RAW, "Heap Address:            %lX\n", (unsigned long)heap_address);
        mlog(RAW, "Name Index:              %lX\n", (unsigned long)name_index);
    }

    if(flags & 0x2)
    {
        uint64_t create_order_index = readField(8); (void)create_order_index;
        if(verbose)
        {
            mlog(RAW, "Creation Order Index:    %lX\n", (unsigned long)create_order_index);
        }
    }
}

/******************************************************************************
 * HDF5 LITE LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Lite::init (void)
{

}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Lite::deinit (void)
{

}

/*----------------------------------------------------------------------------
 * parseUrl
 *----------------------------------------------------------------------------*/
H5Lite::driver_t H5Lite::parseUrl (const char* url, const char** resource)
{
    /* Sanity Check Input */
    if(!url) return UNKNOWN;

    /* Set Resource */
    if(resource) 
    {
        const char* rptr = StringLib::find(url, "//");
        if(rptr)
        {
            *resource = rptr + 2;
        }
    }

    /* Return Driver */
    if(StringLib::find(url, "file://"))
    {
        return FILE;
    }
    else if(StringLib::find(url, "s3://"))
    {
        return S3;
    }
    else if(StringLib::find(url, "hsds://"))    
    {
        return HSDS;
    }
    else
    {
        return UNKNOWN;
    }
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Lite::info_t H5Lite::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows)
{
    info_t info;
    bool status = false;

    do
    {
        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open Resource */
        fileptr_t file = NULL; //H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file == NULL)
        {
            mlog(CRITICAL, "Failed to open resource: %s\n", url);
            break;
        }

        /* Open Dataset */
        int dataset = 0; // H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", datasetname);
            break;
        }

        /* Set Info */
        uint8_t* data = NULL;
        int elements = 0;
        int typesize = 0;
        long datasize = 0;

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s\n", elements, datasize, url, datasetname);
        uint32_t parent_trace_id = TraceLib::grabId();
        uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lite_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

        /* Stop Trace */
        stop_trace(trace_id);

        /* Read Dataset */
        status = true;
        mlog(CRITICAL, "Failed to read data from %s\n", datasetname);


        /* Return Info */
        info.elements = elements;
        info.typesize = typesize;
        info.datasize = datasize;
        info.data = data;
    }
    while(false);


    /* Return Info */
    if(status)  return info;
    else        throw std::runtime_error("H5Lite");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lite::traverse (const char* url, int max_depth, const char* start_group)
{
    bool status = false;

    try
    {
        /* Initialize Recurse Data */
        rdepth_t recurse = {.data = 0};
        recurse.curr.max = max_depth;

        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            throw std::runtime_error("Invalid url");
        }

        /* Open File */
        H5FileBuffer h5file(resource);
        h5file.displayFileInfo();
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to traverse resource: %s\n", e.what());
    }

    /* Return Status */
    return status;
}