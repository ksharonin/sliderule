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

#ifndef __gdal_raster__
#define __gdal_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "OsApi.h"
#include <ogr_spatialref.h>

/******************************************************************************
 * GDALRASTER CLASS
 ******************************************************************************/


// TODO: add bbox as optional parameter with functions to use it
// have the lua_get_raster use this constructor

class GdalRaster: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int GDALRASTER_PIXEL_ON = 1;
        static const int GDALRASTER_MAX_IMAGE_SIZE = 4194304; // 4MB
        static const int GDALRASTER_PHOTON_CRS = 4326;

        static const char* IMAGE_KEY;
        static const char* IMAGELENGTH_KEY;
        static const char* DIMENSION_KEY;
        static const char* BBOX_KEY;
        static const char* CELLSIZE_KEY;
        static const char* CRS_KEY;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            double lon_min;
            double lat_min;
            double lon_max;
            double lat_max;
        } bbox_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);
        static GdalRaster*  create          (lua_State* L, int index);
    
        bool                subset         (double lon, double lat);
        virtual            ~GdalRaster     (void);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        bool rawPixel (const uint32_t row, const uint32_t col)
        {
            return raster[(row * cols) + col] == GDALRASTER_PIXEL_ON;
        }

        uint32_t numRows (void)
        {
            return rows;
        }

        uint32_t numCols (void)
        {
            return cols;
        }

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GdalRaster (lua_State* L, const char* image, long imagelength, bbox_t _bbox, double _cellsize,  uint32_t _epsgl);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        uint32_t    rows;
        uint32_t    cols;
        uint8_t*    raster;
        bbox_t      bbox;
        double      cellsize;
        uint32_t    epsg;
        
    
        OGRCoordinateTransformation* latlon2xy = NULL;
        OGRSpatialReference source;
        OGRSpatialReference target;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions    (lua_State* L);
        static int luaBoundingBox   (lua_State* L);
        static int luaCellSize      (lua_State* L);
        static int luaPixel         (lua_State* L);
        static int luaSubset        (lua_State* L);
};

#endif  /* __gdal_raster__ */
