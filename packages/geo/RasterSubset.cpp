/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University of Washington nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "EventLib.h"
#include "RasterSubset.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

uint64_t RasterSubset::poolsize = RasterSubset::MAX_SIZE;
Mutex RasterSubset::mutex;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterSubset::RasterSubset(uint32_t _cols, uint32_t _rows, RecordObject::fieldType_t _datatype,
                           double ulx, double uly, double _cellsize, const char* _wkt,
                           double _time, double _fileId):
    data(NULL),
    size(0),
    cols(_cols),
    rows(_rows),
    datatype(_datatype),
    map_ulx(ulx),
    map_uly(uly),
    cellsize(_cellsize),
    wkt(_wkt),
    time(_time),
    fileId(_fileId)
{
    /* Calculate Size of Buffer */
    if(datatype >= 0 && datatype <= RecordObject::INVALID_FIELD)
    {
        size = cols * rows * RecordObject::FIELD_TYPE_BYTES[datatype];
    }
    else
    {
        size = 0;
    }

    /* Check for Available Memory */
    bool allocate = false;
    mutex.lock();
    {
        if(size > 0 && size <= poolsize)
        {
            poolsize -= size;
            allocate = true;
        }
    }
    mutex.unlock();

    if(allocate)
    {
        /* Allocate Buffer */
        data = new uint8_t[size];
    }
    else size = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterSubset::~RasterSubset( void )
{
    mutex.lock();
    {
        poolsize += size;
    }
    mutex.unlock();
    if(data) delete [] data;
}
