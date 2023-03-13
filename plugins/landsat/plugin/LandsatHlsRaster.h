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

#ifndef __landsat_hls_raster__
#define __landsat_hls_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <string>
#include "VctRaster.h"

/******************************************************************************
 * ARCTICDEM STRIPS RASTER CLASS
 ******************************************************************************/

class LandsatHlsRaster: public VctRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/
        static const char* L8_bands[];     /* Landsat 8  */
        static const char* S2_bands[];     /* Sentinel 2 */
        static const char* ALGO_names[];   /* Algorithms names */
        static const char* ALGO_bands[];   /* Algorithms bands */

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            LANDSAT8  = 0,
            SENTINEL2 = 1,
            ALGOBAND  = 2,
            ALGONAME  = 3
        } band_type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static GeoRaster* create(lua_State* L, GeoParms* _parms)
                          { return new LandsatHlsRaster(L, _parms); }


    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                LandsatHlsRaster (lua_State* L, GeoParms* _parms);

        void    getIndexFile     (std::string& file, double lon=0, double lat=0 );
        void    getIndexBbox     (bbox_t& bbox, double lon=0, double lat=0);
        bool    findRasters      (OGRPoint &p);
        int     getSamples       (double lon, double lat, List<sample_t>& slist, void* param=NULL);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

    private:
        bool validateBand   (band_type_t type, const char* bandName);

        inline bool isValidL8Band   (const char* bandName) {return validateBand(LANDSAT8, bandName);}
        inline bool isValidS2Band   (const char* bandName) {return validateBand(SENTINEL2,bandName);}
        inline bool isValidAlgoBand (const char* bandName) {return validateBand(ALGOBAND, bandName);}
        inline bool isValidAlgoName (const char* bandName) {return validateBand(ALGONAME, bandName);}

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        std::string filePath;
        std::string indexFile;
        Mutex samplingMutex;

        Dictionary<bool> bandsDict;

        bool ndsi;
        bool ndvi;
        bool ndwi;
};

#endif  /* __landsat_hls_raster__ */