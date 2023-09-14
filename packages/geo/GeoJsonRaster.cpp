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

#include "GeoJsonRaster.h"
#include <gdalwarper.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoJsonRaster::FILEDATA_KEY   = "data";
const char* GeoJsonRaster::CELLSIZE_KEY   = "cellsize";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      file=<file>,
 *      filelength=<filelength>,
 *  })
 *----------------------------------------------------------------------------*/
int GeoJsonRaster::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
        return returnLuaStatus(L, false);
    }
}


/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoJsonRaster* GeoJsonRaster::create (lua_State* L, int index)
{
    /* Get geojson file */
    lua_getfield(L, index, FILEDATA_KEY);
    const char* geojstr = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get cellsize */
    lua_getfield(L, index, CELLSIZE_KEY);
    double _cellsize = getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get Geo Parameters */
    lua_getfield(L, index, GeoParms::SELF);
    GeoParms* _parms = new GeoParms(L, lua_gettop(L), true);
    LuaObject::referenceLuaObject(_parms); // GeoJsonRaster expects a LuaObject created from a Lua script
    lua_pop(L, 1);

    /* Create GeoJsonRaster */
    return new GeoJsonRaster(L, _parms, geojstr, _cellsize);
}


/*----------------------------------------------------------------------------
 * includes
 *----------------------------------------------------------------------------*/
bool GeoJsonRaster::includes(double lon, double lat, double height)
{
    std::vector<RasterSample> slist;
    OGRPoint poi(lon, lat, height);
    int sampleCnt = 0;

    try
    {
        getSamples(&poi, 0, slist);
        sampleCnt = slist.size();
    }
    catch(const RunTimeException& e)
    {
        /* Not likely to happen, GeoJsonRaster class creates it's own raster in vsimemory */
        mlog(e.level(), "%s", e.what());
    }

    if( sampleCnt == 0 ) return false;
    if( sampleCnt > 1  ) mlog(ERROR, "Multiple samples returned for lon: %.2lf, lat: %.2lf, using first sample", lon, lat);

    return (static_cast<int>(slist[0].value) == RASTER_PIXEL_ON);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::~GeoJsonRaster(void)
{
    VSIUnlink(rasterFileName.c_str());
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::GeoJsonRaster(lua_State* L, GeoParms* _parms, const char* geojstr, double _cellsize):
 GeoRaster(L, _parms, std::string("/vsimem/" + GdalRaster::getUUID() + ".tif"), TimeLib::gpstime(), false /* not elevation*/)
{
    bool rasterCreated = false;
    GDALDataset* rasterDset = NULL;
    GDALDataset* jsonDset   = NULL;
    const std::string jsonFile = "/vsimem/" + GdalRaster::getUUID() + ".geojson";
    rasterFileName = getFileName();

    if (geojstr == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (_cellsize <= 0.0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid cellSize: %.2lf:", _cellsize);

    try
    {
        /* Create raster from geojson file */
        vsi_l_offset len = strlen(geojstr);
        VSILFILE* fp = VSIFileFromMemBuffer(jsonFile.c_str(), (GByte*)geojstr, len, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);

        jsonDset = (GDALDataset *)GDALOpenEx(jsonFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(jsonDset);
        OGRLayer *srcLayer = jsonDset->GetLayer(0);
        CHECKPTR(srcLayer);

        OGREnvelope e;
        OGRErr ogrerr = srcLayer->GetExtent(&e);
        CHECK_GDALERR(ogrerr);

        double cellsize = _cellsize;
        int cols = int((e.MaxX - e.MinX) / cellsize);
        int rows = int((e.MaxY - e.MinY) / cellsize);

        char **options = NULL;
        options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");

        GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        CHECKPTR(driver);
        rasterDset = (GDALDataset *)driver->Create(rasterFileName.c_str(), cols, rows, 1, GDT_Byte, options);
        CSLDestroy(options);
        CHECKPTR(rasterDset);
        double geot[6] = {e.MinX, cellsize, 0, e.MaxY, 0, -cellsize};
        rasterDset->SetGeoTransform(geot);

        OGRSpatialReference *srcSrs = srcLayer->GetSpatialRef();
        CHECKPTR(srcSrs);

        char *wkt;
        ogrerr = srcSrs->exportToWkt(&wkt);
        CHECK_GDALERR(ogrerr);
        rasterDset->SetProjection(wkt);
        CPLFree(wkt);

        int bandInx = 1; /* Band index starts at 1, not 0 */
        GDALRasterBand *rb = rasterDset->GetRasterBand(bandInx);
        CHECKPTR(rb);
        rb->SetNoDataValue(RASTER_NODATA_VALUE);

        /*
         * Build params for GDALRasterizeLayers
         * Raster with 1 band, using first layer from json vector
         */
        const int BANDCNT = 1;

        int bandlist[BANDCNT];
        bandlist[0] = bandInx;

        OGRLayer *layers[BANDCNT];
        layers[0] = srcLayer;

        double burnValues[BANDCNT];
        burnValues[0] = RASTER_PIXEL_ON;

        CPLErr cplerr = GDALRasterizeLayers(rasterDset, 1, bandlist, 1, (OGRLayerH *)&layers[0], NULL, NULL, burnValues, NULL, NULL, NULL);
        CHECK_GDALERR(cplerr);
        mlog(DEBUG, "Rasterized geojson into raster %s", rasterFileName.c_str());

        /* Must close raster to flush it into file in vsimem */
        GDALClose((GDALDatasetH)rasterDset);
        rasterDset = NULL;
        rasterCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
    }

   /* Cleanup */
   VSIUnlink(jsonFile.c_str());
   if(jsonDset) GDALClose((GDALDatasetH)jsonDset);
   if(rasterDset) GDALClose((GDALDatasetH)rasterDset);

   if(!rasterCreated)
       throw RunTimeException(CRITICAL, RTE_ERROR, "GeoJsonRaster failed");
}