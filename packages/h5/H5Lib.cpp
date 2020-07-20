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

#include <hdf5.h>
#ifdef H5_USE_REST_VOL
#include <rest_vol_public.h>
#endif

#include "H5Lib.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>

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

hid_t file_access_properties = H5P_DEFAULT;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * hdf5_iter_op_func
 *----------------------------------------------------------------------------*/
herr_t hdf5_iter_op_func (hid_t loc_id, const char* name, const H5L_info_t* info, void* operator_data)
{
    (void)info;

    herr_t      retval = 0;
    rdepth_t    recurse = { .data = (uint64_t)operator_data };

    for(unsigned i = 0; i < recurse.curr.depth; i++) mlog(RAW, "  ");

    H5O_info_t object_info;
    H5Oget_info_by_name(loc_id, name, &object_info, H5P_DEFAULT);
    switch (object_info.type)
    {
        case H5O_TYPE_GROUP:
        {
            H5L_info_t link_info;
            H5Lget_info(loc_id, name, &link_info, H5P_DEFAULT);
            if(link_info.type == H5L_TYPE_HARD)
            {
                mlog(RAW, "%s: {", name);
                recurse.curr.depth++;
                if(recurse.curr.depth < recurse.curr.max)
                {
                    mlog(RAW, "\n");
                    retval = H5Literate_by_name(loc_id, name, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data, H5P_DEFAULT);
                    for(unsigned i = 0; i < recurse.curr.depth - 1; i++) mlog(RAW, "  ");
                    mlog(RAW, "}\n");
                }
                else
                {
                    mlog(RAW, " }\n");
                }
            }
            else
            {
                mlog(RAW, "*%s\n", name);
            }
            break;
        }
        case H5O_TYPE_DATASET:
        {
            mlog(RAW, "%s\n", name);
            break;
        }
        case H5O_TYPE_NAMED_DATATYPE:
        {
            mlog(RAW, "%s (type)\n", name);
            break;
        }
        default:
        {
            mlog(RAW, "%s (unknown)\n", name);
        }
    }

    return retval;
}

/******************************************************************************
 * HDF5 LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Lib::init (void)
{
    #ifdef H5_USE_REST_VOL
        H5rest_init();
        file_access_properties = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_rest_vol(file_access_properties);
    #endif
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Lib::deinit (void)
{
    #ifdef H5_USE_REST_VOL
        H5Pclose(file_access_properties);
        H5rest_term();
    #endif
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Lib::info_t H5Lib::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, int col, int maxrows)
{
    info_t info;
    bool status = false;

    /* Start Trace */
    uint32_t parent_trace_id = TraceLib::grabId();
    uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lib_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

    /* Start with Invalid Handles */
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t filespace = H5S_ALL;
    hid_t memspace = H5S_ALL;
    hid_t datatype = INVALID_RC;
    bool datatype_allocated = false;

    do
    {
        mlog(INFO, "Opening resource: %s\n", url);
        file = H5Fopen(url, H5F_ACC_RDONLY, file_access_properties);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s\n", url);
            break;
        }

        /* Open Dataset */
        dataset = H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", datasetname);
            break;
        }

        /* Open Dataspace */
        filespace = H5Dget_space(dataset);
        if(filespace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s\n", datasetname);
            break;
        }

        /* Get Datatype */
        if(valtype == RecordObject::INTEGER)
        {
            datatype = H5T_NATIVE_INT;
        }
        else if(valtype == RecordObject::REAL)
        {
            datatype = H5T_NATIVE_DOUBLE;
        }
        else
        {
            datatype = H5Dget_type(dataset);
            datatype_allocated = true;
        }

        /* Get Type Size */
        size_t typesize = H5Tget_size(datatype);

        /* Get Dimensions of Data */
        int ndims = H5Sget_simple_extent_ndims(filespace);
        hsize_t* dims = new hsize_t[ndims];
        H5Sget_simple_extent_dims(filespace, dims, NULL);

        /* Select Specific Column */
        if( ((col >= 0) && (ndims == 2)) || ((col == 0) && (ndims == 1)) )
        {
            /* Allocate Hyperspace Parameters */
            hsize_t* start = new hsize_t[ndims];
            hsize_t* count = new hsize_t[ndims];

            /* Create File Hyperspace to Read Selected Column */
            start[0] = 0;
            start[1] = col;
            count[0] = MAX(maxrows, (int)dims[0]);
            count[1] = 1;
            H5Sselect_hyperslab(filespace, H5S_SELECT_SET, start, NULL, count, NULL);

            /* Create Memory Hyperspace to Write Selected Column */
            dims[1] = 1; // readjust dimensions to reflect single column being read
            start[1] = 0; // readjust start to reflect writing to only a single column
            memspace = H5Screate_simple(ndims, dims, NULL);
            H5Sselect_hyperslab(memspace, H5S_SELECT_SET, start, NULL, count, NULL);

            /* Free Hyperspace Parameters */
            delete [] start;
            delete [] count;
        }
        else if(col >= 0)
        {
            mlog(CRITICAL, "Unsupported column selection of %d on dataset of rank %d\n", col, ndims);
            break;
        }

        /* Get Number of Elements */
        int elements = 1;
        for(int d = 0; d < ndims; d++)
        {
            elements *= (int32_t)dims[d];
        }

        /* Get Size of Data */
        long datasize = elements * typesize;

        /* Allocate Data Buffer */
        uint8_t* data = NULL;
        try
        {
            data = new uint8_t[datasize];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", elements);
            break;
        }

        /* Read Dataset */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s\n", elements, datasize, datasetname);
        if(H5Dread(dataset, datatype, memspace, filespace, H5P_DEFAULT, data) >= 0)
        {
            status = true;
        }
        else
        {
            mlog(CRITICAL, "Failed to read data from %s\n", datasetname);
            delete [] data;
            break;
        }

        /* Set Info Return Structure */
        info.elements = elements;
        info.typesize = typesize;
        info.datasize = datasize;
        info.data = data;
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(datatype_allocated && datatype > 0) H5Tclose(datatype);
    if(filespace != H5S_ALL) H5Sclose(filespace);
    if(memspace != H5S_ALL) H5Sclose(memspace);
    if(dataset > 0) H5Dclose(dataset);

    /* Stop Trace */
    stop_trace(trace_id);

    /* Return Info */
    if(status)  return info;
    else        throw std::runtime_error("H5Lib");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lib::traverse (const char* url, int max_depth, const char* start_group)
{
    bool        status = false;
    hid_t       file = INVALID_RC;
    hid_t       group = INVALID_RC;

    do
    {
        /* Initialize Recurse Data */
        rdepth_t recurse = {.data = 0};
        recurse.curr.max = max_depth;

        /* Open File */
        file = H5Fopen(url, H5F_ACC_RDONLY, file_access_properties);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s", url);
            break;
        }

        /* Open Group */
        if(start_group)
        {
            group = H5Gopen(file, start_group, H5P_DEFAULT);
            if(group < 0)
            {
                mlog(CRITICAL, "Failed to open group: %s", start_group);
                break;
            }
        }

        /* Display File Structure */
        if(H5Literate(group > 0 ? group : file, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data) >= 0)
        {
            status = true;
        }
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(group > 0) H5Gclose(group);

    /* Return Status */
    return status;
}
