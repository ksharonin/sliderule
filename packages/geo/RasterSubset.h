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

#ifndef __raster_subset__
#define __raster_subset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "RecordObject.h"

/******************************************************************************
 * RASTER SUBSET CLASS
 ******************************************************************************/

class RasterSubset
{
    public:

        uint8_t*                    data;
        uint64_t                    size;
        uint64_t                    cols;
        uint64_t                    rows;
        RecordObject::fieldType_t   datatype;
        double                      minx;  // Extent for subset data
        double                      miny;
        double                      maxx;
        double                      maxy;
        double                      cellsize;
        double                      time;     // gps seconds
        uint64_t                    fileId;

        static const uint64_t       oneGB    = 0x40000000;
        static const uint64_t       MAX_SIZE = oneGB * 6;

        static uint64_t             poolsize;
        static Mutex                mutex;

        RasterSubset(uint32_t _cols, uint32_t _rows, RecordObject::fieldType_t _datatype,
                     double _minx, double _miny, double _maxx, double _maxy, double _cellsize,
                     double _time, double _fileId);
        ~RasterSubset(void);
};

#endif /* __raster_subset__ */