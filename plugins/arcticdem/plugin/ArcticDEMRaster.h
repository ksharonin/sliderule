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

#ifndef __arcticdem_raster__
#define __arcticdem_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "OsApi.h"
#include <ogr_geometry.h>

/******************************************************************************
 * GEOJSON RASTER CLASS
 ******************************************************************************/

class ArcticDEMRaster: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/
        static const int   ARCTIC_DEM_INVALID_EL = -1000000;
        static const int   RASTER_NODATA_VALUE = 200;
        static const int   RASTER_PIXEL_ON = 1;
        static const int   RASTER_MAX_IMAGE_SIZE = 4194304; // 4MB
        static const int   RASTER_PHOTON_CRS = 4326;

        static const char* FILEDATA_KEY;
        static const char* FILELENGTH_KEY;
        static const char* BBOX_KEY;
        static const char* CELLSIZE_KEY;

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

        static void             init           (void);
        static void             deinit         (void);
        static int              luaCreate      (lua_State* L);
        static ArcticDEMRaster* create         (lua_State* L, int index);

        float                   subset         (double lon, double lat);
        bool                    createRaster   (OGRPoint* p);
        virtual                ~ArcticDEMRaster(void);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        float rawPixel (const uint32_t row, const uint32_t col)
        {
            return raster[(row * cols) + col];
        }

        uint32_t numRows (void)
        {
            return rows;
        }

        uint32_t numCols(void)
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

        ArcticDEMRaster (lua_State* L, const char* image, long imagelength);

    private:
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        const std::string indexfname = "/data/ArcticDEM/ArcticDEM_Tile_Index_Rel7/ArcticDEM_Tile_Index_Rel7.shp";

        float    *raster;
        uint32_t  rows;
        uint32_t  cols;
        bbox_t    bbox;
        double    cellsize;

        OGRCoordinateTransformation *latlon2xy;
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

#endif  /* __arcticdem_raster__ */
