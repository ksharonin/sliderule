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

#ifndef __h5_lite__
#define __h5_lite__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordObject.h"
#include "List.h"
#include "Table.h"

/******************************************************************************
 * HDF5 FILE BUFFER CLASS
 ******************************************************************************/

class H5FileBuffer
{
    public:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const long ALL_ROWS  = -1;
        static const int MAX_NDIMS  = 2;
    
        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/
        
        typedef enum {
            DATASPACE_MSG           = 0x1,
            LINK_INFO_MSG           = 0x2,
            DATATYPE_MSG            = 0x3,
            FILL_VALUE_MSG          = 0x5,
            LINK_MSG                = 0x6,
            DATA_LAYOUT_MSG         = 0x8,
            FILTER_MSG              = 0xB,
            HEADER_CONT_MSG         = 0x10,
            SYMBOL_TABLE_MSG        = 0x11
        } msg_type_t;

        typedef enum {
            FIXED_POINT_TYPE        = 0,
            FLOATING_POINT_TYPE     = 1,
            TIME_TYPE               = 2,
            STRING_TYPE             = 3,
            BIT_FIELD_TYPE          = 4,
            OPAQUE_TYPE             = 5,
            COMPOUND_TYPE           = 6,
            REFERENCE_TYPE          = 7,
            ENUMERATED_TYPE         = 8,
            VARIABLE_LENGTH_TYPE    = 9,
            ARRAY_TYPE              = 10,
            UNKNOWN_TYPE            = 11
        } data_type_t;

        typedef enum {
            COMPACT_LAYOUT          = 0,
            CONTIGUOUS_LAYOUT       = 1,
            CHUNKED_LAYOUT          = 2,
            UNKNOWN_LAYOUT          = 3
        } layout_t;

        typedef enum {
            INVALID_FILTER          = 0,
            DEFLATE_FILTER          = 1,
            SHUFFLE_FILTER          = 2,
            FLETCHER32_FILTER       = 3,
            SZIP_FILTER             = 4,
            NBIT_FILTER             = 5,
            SCALEOFFSET_FILTER      = 6,
            NUM_FILTERS             = 7
        } filter_t;

        typedef struct {
            int                     elements;   // number of elements in dataset
            int                     typesize;   // number of bytes per element
            int                     datasize;   // total number of bytes in dataset
            uint8_t*                data;       // point to allocated data buffer
            /* h5lite specific */
            RecordObject::valType_t datatype;   // high level data type
            int                     numcols;    // number of columns - anything past the second dimension is grouped togeher
            int                     numrows;    // number of rows - includes all dimensions after the first as a single row
        } dataset_info_t;
    
        typedef struct {
            uint8_t*                data;
            int64_t                 size;
            uint64_t                pos;
        } cache_entry_t;

        typedef Table<cache_entry_t, uint64_t> cache_t;

        /*--------------------------------------------------------------------
        * I/O Context (subclass)
        *--------------------------------------------------------------------*/

        struct io_context_t
        {
            cache_t     l1; // level 1 cache
            cache_t     l2; // level 2 cache
            Mutex       mut; // cache mutex

            io_context_t    (void);
            ~io_context_t   (void);
        };

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

                            H5FileBuffer        (dataset_info_t* data_info, io_context_t* context, const char* filename, const char* _dataset, long startrow, long numrows, bool _error_checking=false, bool _verbose=false);
        virtual             ~H5FileBuffer       (void);

    protected:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        /* 
         * Assuming:
         *  50 regions of interest
         *  100 granuels per region
         *  30 datasets per granule
         *  200 bytes per dataset
         * Then:
         *  15000 datasets are needed
         *  30MB of data is used
         */ 

        static const long       MAX_META_STORE          = 150000;
        static const long       MAX_META_FILENAME       = 88; // must be multiple of 8

        /*
         * Assuming:
         *  50ms of latency per read
         * Then per throughput:
         *  ~500Mbits/second --> 1MB (L1 LINESIZE)
         *  ~2Gbits/second --> 8MB (L1 LINESIZE)
         */

        static const long       IO_CACHE_L1_LINESIZE    = 0x100000; // 1MB cache line
        static const long       IO_CACHE_L1_MASK        = 0x0FFFFF; // lower inverse of buffer size
        static const long       IO_CACHE_L1_ENTRIES     = 157; // cache lines per dataset

        static const long       IO_CACHE_L2_LINESIZE    = 0x8000000; // 128MB cache line
        static const long       IO_CACHE_L2_MASK        = 0x7FFFFFF; // lower inverse of buffer size
        static const long       IO_CACHE_L2_ENTRIES     = 17; // cache lines per dataset

        static const long       STR_BUFF_SIZE           = 128;

        static const uint64_t   H5_SIGNATURE_LE         = 0x0A1A0A0D46444889LL;
        static const uint64_t   H5_OHDR_SIGNATURE_LE    = 0x5244484FLL; // object header
        static const uint64_t   H5_FRHP_SIGNATURE_LE    = 0x50485246LL; // fractal heap
        static const uint64_t   H5_FHDB_SIGNATURE_LE    = 0x42444846LL; // direct block
        static const uint64_t   H5_FHIB_SIGNATURE_LE    = 0x42494846LL; // indirect block
        static const uint64_t   H5_OCHK_SIGNATURE_LE    = 0x4B48434FLL; // object header continuation block
        static const uint64_t   H5_TREE_SIGNATURE_LE    = 0x45455254LL; // binary tree version 1
        static const uint64_t   H5_HEAP_SIGNATURE_LE    = 0x50414548LL; // local heap
        static const uint64_t   H5_SNOD_SIGNATURE_LE    = 0x444F4E53LL; // symbol table
        
        static const uint8_t    H5LITE_CUSTOM_V1_FLAG   = 0x80; // used to indicate version 1 object header (reserved)
        
        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t                chunk_size;
            uint32_t                filter_mask;
            uint64_t                slice[MAX_NDIMS];
            uint64_t                row_key;
        } btree_node_t;

        typedef struct {
            int                     table_width;
            int                     curr_num_rows;
            int                     starting_blk_size;
            int                     max_dblk_size;
            int                     blk_offset_size;  // size in bytes of block offset field
            bool                    dblk_checksum;
            msg_type_t              msg_type;
            int                     num_objects;
            int                     cur_objects; // mutable
        } heap_info_t;

        typedef union {
            double                  fill_lf;
            float                   fill_f;
            uint64_t                fill_ll;
            uint32_t                fill_l;
            uint16_t                fill_s;
            uint8_t                 fill_b;
        } fill_t;

        typedef struct {
            char                    url[MAX_META_FILENAME];
            data_type_t             type;
            layout_t                layout;
            fill_t                  fill;
            bool                    filter[NUM_FILTERS]; // true if enabled for dataset
            int                     typesize;
            int                     fillsize;
            int                     ndims;
            int                     elementsize; // size of the data element in the chunk; should be equal to the typesize
            uint64_t                dimensions[MAX_NDIMS];
            uint64_t                chunkelements; // number of data elements per chunk
            uint64_t                address;
            int64_t                 size;
        } meta_entry_t;

        typedef Table<meta_entry_t, uint64_t> meta_repo_t;
        
       /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/
        
        void                ioOpen              (const char* filename);
        void                ioClose             (void);
        int64_t             ioRead              (uint8_t* data, int64_t size, uint64_t pos);

        uint8_t*            ioRequest           (int64_t size, uint64_t* pos, int64_t hint=IO_CACHE_L1_LINESIZE, bool* cached=NULL);
        bool                ioCheckCache        (int64_t size, uint64_t pos, cache_t* cache, long line_mask, cache_entry_t* entry);
        static uint64_t     ioHashL1            (uint64_t key);
        static uint64_t     ioHashL2            (uint64_t key);

        void                readByteArray       (uint8_t* data, int64_t size, uint64_t* pos);
        uint64_t            readField           (int64_t size, uint64_t* pos);
        void                readDataset         (dataset_info_t* _data_info);

        int                 readSuperblock      (void);        
        int                 readFractalHeap     (msg_type_t type, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readDirectBlock     (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readIndirectBlock   (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readBTreeV1         (uint64_t pos, uint8_t* buffer, uint64_t buffer_size, uint64_t buffer_offset);
        btree_node_t        readBTreeNodeV1     (int ndims, uint64_t* pos);
        int                 readSymbolTable     (uint64_t pos, uint64_t heap_data_addr, int dlvl);

        int                 readObjHdr          (uint64_t pos, int dlvl);
        int                 readMessages        (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readObjHdrV1        (uint64_t pos, int dlvl);
        int                 readMessagesV1      (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl);
        int                 readMessage         (msg_type_t type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl);

        int                 readDataspaceMsg    (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readLinkInfoMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readDatatypeMsg     (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readFillValueMsg    (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readLinkMsg         (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readDataLayoutMsg   (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readFilterMsg       (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readHeaderContMsg   (uint64_t pos, uint8_t hdr_flags, int dlvl);
        int                 readSymbolTableMsg  (uint64_t pos, uint8_t hdr_flags, int dlvl);

        void                parseDataset        (const char* _dataset);
        const char*         type2str            (data_type_t datatype);
        const char*         layout2str          (layout_t layout);
        int                 highestBit          (uint64_t value);
        int                 inflateChunk        (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size);
        int                 shuffleChunk        (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_offset, uint32_t output_size, int type_size);

        static uint64_t     metaHash            (const char* url);
        static void         metaUrl             (char* url, const char* filename, const char* _dataset);

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        /* Meta Repository */
        static meta_repo_t  metaRepo;
        static Mutex        metaMutex;

        /* Class Data */
        const char*         dataset;
        const char*         datasetPrint;
        List<const char*>   datasetPath;
        uint64_t            datasetStartRow;
        int                 datasetNumRows;
        bool                errorChecking;
        bool                verbose;

        /* I/O Management */
        fileptr_t           ioFile;
        io_context_t*       ioContext;
        bool                ioContextLocal;

        /* File Info */
        int                 offsetSize;
        int                 lengthSize;
        uint64_t            rootGroupOffset;
        uint8_t*            dataChunkBuffer; // buffer for reading uncompressed chunk
        int64_t             dataChunkBufferSize; // dataChunkElements * dataInfo->typesize 
        int                 highestDataLevel; // high water mark for traversing dataset path
        int64_t             dataSizeHint;

        /* Meta Info */
        meta_entry_t        metaData;
};

/******************************************************************************
 * HDF5 I/O LITE LIBRARY
 ******************************************************************************/

struct H5Lite
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/
    
    static const long ALL_ROWS = H5FileBuffer::ALL_ROWS;

    typedef enum {
        FILE,
        HSDS,
        S3,
        UNKNOWN
    } driver_t;

    typedef H5FileBuffer::dataset_info_t info_t;

    typedef H5FileBuffer::io_context_t context_t;

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

    static void         init            (void);
    static void         deinit          (void);

    static driver_t     parseUrl        (const char* url, const char** resource);
    static info_t       read            (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context=NULL);
    static bool         traverse        (const char* url, int max_depth, const char* start_group);
};

#endif  /* __h5_lite__ */
