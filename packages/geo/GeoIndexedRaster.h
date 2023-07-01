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

#ifndef __geo_indexed_raster__
#define __geo_indexed_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GdalRaster.h"
#include "RasterObject.h"


/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GeoIndexedRaster: public RasterObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   MAX_READER_THREADS = 200;
        static const int   MAX_CACHED_RASTERS = 50;

        static const char* FLAGS_RASTER_TAG;
        static const char* SAMPLES_RASTER_TAG;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            std::string         tag;
            std::string         fileName;
        } raster_info_t;

        typedef struct {
            std::string             wkt;
            std::string             id;
            Ordering<raster_info_t> list;
            TimeLib::gmt_time_t     gmtDate;
            int64_t                 gpsTime;
        } rasters_group_t;

        typedef struct {
            Thread*     thread;
            GdalRaster* raster;
            Cond*       sync;
            bool        run;
        } reader_t;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual         ~GeoIndexedRaster  (void);
        void            getSamples  (double lon, double lat, double height, int64_t gps, List<RasterSample>& slist, void* param=NULL) override;
        static void     init        (void);
        static void     deinit      (void);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoIndexedRaster      (lua_State* L, GeoParms* _parms);
        virtual void    getGroupSamples       (const rasters_group_t& rgroup, List<RasterSample>& slist, uint32_t flags);
        double          getGmtDate            (const OGRFeature* feature, const char* field,  TimeLib::gmt_time_t& gmtDate);
        void            openGeoIndex          (double lon = 0, double lat = 0);
        virtual void    getIndexFile          (std::string& file, double lon, double lat) = 0;
        virtual bool    findRasters           (GdalRaster::Point& p) = 0;
        void            sampleRasters         (void);
        int             sample                (double lon, double lat, double height, int64_t gps);

        bool containsPoint(GdalRaster::Point& poi)
        {
            return (indexDset &&
                    (poi.x >= bbox.lon_min) && (poi.x <= bbox.lon_max) &&
                    (poi.y >= bbox.lat_min) && (poi.y <= bbox.lat_max));
        }


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Mutex                       samplingMutex;
        Ordering<rasters_group_t>*  rasterGroupList;
        Dictionary<GdalRaster*>     rasterDict;
        List<OGRFeature*>           featuresList;
        bool                        forceNotElevation;
        OGRLayer *layer;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DATA_TO_SAMPLE = 0;
        static const int DATA_SAMPLED = 1;
        static const int NUM_SYNC_SIGNALS = 2;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        reader_t*          rasterRreader;
        uint32_t           readerCount;

        std::string        indexFile;
        GDALDataset       *indexDset;
        GdalRaster::bbox_t bbox;
        uint32_t           rows;
        uint32_t           cols;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State* L);
        static int luaBoundingBox(lua_State* L);
        static int luaCellSize(lua_State* L);

        static void* readingThread (void *param);

        bool       filterRasters           (int64_t gps);
        void       createThreads           (void);
        void       updateCache             (GdalRaster::Point& p);
        void       invalidateCache         (void);
        int        getSampledRastersCount  (void);
        uint32_t   removeOldestRasterGroup (void);
};




#endif  /* __geo_indexed_raster__ */
