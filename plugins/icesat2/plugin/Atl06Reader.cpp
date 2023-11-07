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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "core.h"
#include "h5.h"

#include "Atl06Reader.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl06Reader::elRecType = "atl06srec.elevation";
const RecordObject::fieldDef_t Atl06Reader::elRecDef[] = {
    {"time",                    RecordObject::TIME8,    offsetof(elevation_t, time_ns),                 1,  NULL, NATIVE_FLAGS},
    {"h_li",                    RecordObject::FLOAT,    offsetof(elevation_t, h_li),                    1,  NULL, NATIVE_FLAGS},
    {"h_li_sigma",              RecordObject::FLOAT,    offsetof(elevation_t, h_li_sigma),              1,  NULL, NATIVE_FLAGS},
    {"latitude",                RecordObject::DOUBLE,   offsetof(elevation_t, latitude),                1,  NULL, NATIVE_FLAGS},
    {"longitude",               RecordObject::DOUBLE,   offsetof(elevation_t, longitude),               1,  NULL, NATIVE_FLAGS},
    {"atl06_quality_summary",   RecordObject::INT8,     offsetof(elevation_t, atl06_quality_summary),   1,  NULL, NATIVE_FLAGS},
    {"segment_id",              RecordObject::UINT32,   offsetof(elevation_t, segment_id),              1,  NULL, NATIVE_FLAGS},
    {"sigma_geo_h",             RecordObject::FLOAT,    offsetof(elevation_t, sigma_geo_h),             1,  NULL, NATIVE_FLAGS},
// ground track
    {"x_atc",                   RecordObject::DOUBLE,   offsetof(elevation_t, x_atc),                   1,  NULL, NATIVE_FLAGS},
    {"y_atc",                   RecordObject::DOUBLE,   offsetof(elevation_t, y_atc),                   1,  NULL, NATIVE_FLAGS},
    {"seg_azimuth",             RecordObject::FLOAT,    offsetof(elevation_t, seg_azimuth),             1,  NULL, NATIVE_FLAGS},
// fit_statistics
    {"dh_fit_dx",               RecordObject::FLOAT,    offsetof(elevation_t, dh_fit_dx),               1,  NULL, NATIVE_FLAGS},
    {"h_robust_sprd",           RecordObject::FLOAT,    offsetof(elevation_t, h_robust_sprd),           1,  NULL, NATIVE_FLAGS},
    {"n_fit_photons",           RecordObject::INT32,    offsetof(elevation_t, n_fit_photons),           1,  NULL, NATIVE_FLAGS},
    {"w_surface_window_final",  RecordObject::FLOAT,    offsetof(elevation_t, w_surface_window_final),  1,  NULL, NATIVE_FLAGS},
// geophysical
    {"bsnow_conf",              RecordObject::INT8,     offsetof(elevation_t, bsnow_conf),              1,  NULL, NATIVE_FLAGS},
    {"bsnow_h",                 RecordObject::FLOAT,    offsetof(elevation_t, bsnow_h),                 1,  NULL, NATIVE_FLAGS},
    {"r_eff",                   RecordObject::FLOAT,    offsetof(elevation_t, r_eff),                   1,  NULL, NATIVE_FLAGS},
    {"tide_ocean",              RecordObject::FLOAT,    offsetof(elevation_t, tide_ocean),              1,  NULL, NATIVE_FLAGS},
};

const char* Atl06Reader::atRecType = "atl06srec";
const RecordObject::fieldDef_t Atl06Reader::atRecDef[] = {
    {"elevation",               RecordObject::USER,     offsetof(atl06_t, elevation),               0,  elRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

const char* Atl06Reader::ancFieldRecType = "atl06sanc.field"; // ancillary atl06s record
const RecordObject::fieldDef_t Atl06Reader::ancFieldRecDef[] = {
    {"extent_id",   RecordObject::UINT64,   offsetof(anc_field_t, extent_id),   1,  NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    offsetof(anc_field_t, value),       8,  NULL, NATIVE_FLAGS}
};

const char* Atl06Reader::ancRecType = "atl06sanc"; // ancillary atl06s record
const RecordObject::fieldDef_t Atl06Reader::ancRecDef[] = {
    {"field_index", RecordObject::UINT8,    offsetof(anc_t, field_index),   1,  NULL, NATIVE_FLAGS},
    {"datatype",    RecordObject::UINT8,    offsetof(anc_t, data_type),     1,  NULL, NATIVE_FLAGS},
    {"data",        RecordObject::USER,     offsetof(anc_t, data),          0,  ancFieldRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

const char* Atl06Reader::OBJECT_TYPE = "Atl06Reader";
const char* Atl06Reader::LUA_META_NAME = "Atl06Reader";
const struct luaL_Reg Atl06Reader::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL06 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl06Reader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    Icesat2Parms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = dynamic_cast<Icesat2Parms*>(getLuaObject(L, 4, Icesat2Parms::OBJECT_TYPE));
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Atl06Reader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", Atl06Reader::LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Reader::init (void)
{
    RECDEF(elRecType,       elRecDef,       sizeof(elevation_t),    NULL /* "extent_id" */);
    RECDEF(atRecType,       atRecDef,       sizeof(atl06_t),        NULL);
    RECDEF(ancFieldRecType, ancFieldRecDef, sizeof(anc_field_t),    NULL /* "extent_id" */);
    RECDEF(ancRecType,      ancRecDef,      sizeof(anc_t),          NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Atl06Reader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->read_timeout * 1000)
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.segments_read     = 0;
    stats.extents_filtered  = 0;
    stats.extents_sent      = 0;
    stats.extents_dropped   = 0;
    stats.extents_retried   = 0;

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Read Global Resource Information */
    try
    {
        /* Parse Globals (throws) */
        parseResource(resource, start_rgt, start_cycle, start_region);

        /* Create Readers */
        for(int track = 1; track <= Icesat2Parms::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < Icesat2Parms::NUM_PAIR_TRACKS; pair++)
            {
                if(parms->track == Icesat2Parms::ALL_TRACKS || track == parms->track)
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    info->track = track;
                    info->pair = pair;
                    StringLib::format(info->prefix, 7, "/gt%d%c", info->track, info->pair == 0 ? 'l' : 'r');
                    readerPid[threadCount++] = new Thread(subsettingThread, info);
                }
            }
        }

        /* Check if Readers Created */
        if(threadCount == 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "No reader threads were created, invalid track specified: %d\n", parms->track);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Log Error */
        mlog(e.level(), "Failed to read global information in resource %s: %s", resource, e.what());

        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) LuaEndpoint::generateExceptionStatus(RTE_TIMEOUT, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);
        else LuaEndpoint::generateExceptionStatus(RTE_RESOURCE_DOES_NOT_EXIST, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl06Reader::~Atl06Reader (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Region::Region (info_t* info):
    latitude        (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/latitude").c_str(),  &info->reader->context),
    longitude       (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/longitude").c_str(), &info->reader->context),
    inclusion_mask  {NULL},
    inclusion_ptr   {NULL}
{
    /* Process Area of Interest */
    projected_poly = NULL;
    projection = MathLib::PLATE_CARREE;
    points_in_polygon = info->reader->parms->polygon.length();
    if(points_in_polygon > 0)
    {
        /* Determine Best Projection To Use */
        if(info->reader->parms->polygon[0].lat > 70.0) projection = MathLib::NORTH_POLAR;
        else if(info->reader->parms->polygon[0].lat < -70.0) projection = MathLib::SOUTH_POLAR;

        /* Project Polygon */
        List<MathLib::coord_t>::Iterator poly_iterator(info->reader->parms->polygon);
        projected_poly = new MathLib::point_t [points_in_polygon];
        for(int i = 0; i < points_in_polygon; i++)
        {
            projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
        }
    }

    try
    {
        /* Join Reads */
        latitude.join(info->reader->read_timeout_ms, true);
        longitude.join(info->reader->read_timeout_ms, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->reader->parms->raster != NULL)
        {
            rasterregion(info);
        }
        else if(points_in_polygon > 0)
        {
            polyregion();
        }
        else
        {
            return; // early exit since no subsetting required
        }

        /* Check If Anything to Process */
        if(num_segments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        latitude.trim(first_segment);
        longitude.trim(first_segment);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }

}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl06Reader::Region::cleanup (void)
{
    if(projected_poly)
    {
        delete [] projected_poly;
        projected_poly = NULL;
    }
    
    if(inclusion_mask)
    {
        delete [] inclusion_mask;
        inclusion_mask = NULL;
    }
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl06Reader::Region::polyregion (void)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < latitude.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        MathLib::coord_t segment_coord = {longitude[segment], latitude[segment]};
        MathLib::point_t segment_point = MathLib::coord2point(segment_coord, projection);

        /* Test Inclusion */
        if(MathLib::inpoly(projected_poly, points_in_polygon, segment_point))
        {
            inclusion = true;
        }

        /* Check First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Polygon */
            first_segment_found = true;
            first_segment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* If Coordinate Is NOT In Polygon */
            break; // full extent found!
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        num_segments = segment - first_segment;
    }
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void Atl06Reader::Region::rasterregion (info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;

    /* Check Size */
    if(latitude.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [latitude.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long last_segment = 0;
    int segment = 0;
    while(segment < latitude.size)
    {
        /* Check Inclusion */
        bool inclusion = info->reader->parms->raster->includes(longitude[segment], latitude[segment]);
        inclusion_mask[segment] = inclusion;

        /* Check For First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Raster */
            first_segment_found = true;
            first_segment = segment;
            last_segment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* Update Number of Segments to Current Segment Count */
            last_segment = segment;
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        num_segments = last_segment - first_segment + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_segment];
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Constructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Atl06Data::Atl06Data (info_t* info, const Region& region):
    sc_orient               (info->reader->asset, info->reader->resource,                                "/orbit_info/sc_orient",                                           &info->reader->context),
    delta_time              (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/delta_time").c_str(),                           &info->reader->context, 0, region.first_segment, region.num_segments),
    h_li                    (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/h_li").c_str(),                                 &info->reader->context, 0, region.first_segment, region.num_segments),
    h_li_sigma              (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/h_li_sigma").c_str(),                           &info->reader->context, 0, region.first_segment, region.num_segments),
    atl06_quality_summary   (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/atl06_quality_summary").c_str(),                &info->reader->context, 0, region.first_segment, region.num_segments),
    segment_id              (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/segment_id").c_str(),                           &info->reader->context, 0, region.first_segment, region.num_segments),
    sigma_geo_h             (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/sigma_geo_h").c_str(),                          &info->reader->context, 0, region.first_segment, region.num_segments),
    x_atc                   (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/x_atc").c_str(),                   &info->reader->context, 0, region.first_segment, region.num_segments),
    y_atc                   (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/y_atc").c_str(),                   &info->reader->context, 0, region.first_segment, region.num_segments),
    seg_azimuth             (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/seg_azimuth").c_str(),             &info->reader->context, 0, region.first_segment, region.num_segments),
    dh_fit_dx               (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/dh_fit_dx").c_str(),             &info->reader->context, 0, region.first_segment, region.num_segments),
    h_robust_sprd           (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/h_robust_sprd").c_str(),         &info->reader->context, 0, region.first_segment, region.num_segments),
    n_fit_photons           (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/n_fit_photons").c_str(),         &info->reader->context, 0, region.first_segment, region.num_segments),
    w_surface_window_final  (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/w_surface_window_final").c_str(),&info->reader->context, 0, region.first_segment, region.num_segments),
    bsnow_conf              (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/bsnow_conf").c_str(),               &info->reader->context, 0, region.first_segment, region.num_segments),
    bsnow_h                 (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/bsnow_h").c_str(),                  &info->reader->context, 0, region.first_segment, region.num_segments),
    r_eff                   (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/r_eff").c_str(),                    &info->reader->context, 0, region.first_segment, region.num_segments),
    tide_ocean              (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/tide_ocean").c_str(),               &info->reader->context, 0, region.first_segment, region.num_segments)
{
    Icesat2Parms::string_list_t* anc_fields = info->reader->parms->atl06_fields;

    /* Read Ancillary Fields */
    if(anc_fields)
    {
        for(int i = 0; i < anc_fields->length(); i++)
        {
            const char* field_name = (*anc_fields)[i].c_str();
            FString dataset_name("%s/land_ice_segments/%s", info->prefix, field_name);
            H5DArray* array = new H5DArray(info->reader->asset, info->reader->resource, dataset_name.c_str(), &info->reader->context, 0, region.first_segment, region.num_segments);
            bool status = anc_data.add(field_name, array);
            if(!status) delete array;
            assert(status); // the dictionary add should never fail
        }
    }

    /* Join Hardcoded Reads */
    sc_orient.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    h_li.join(info->reader->read_timeout_ms, true);
    h_li_sigma.join(info->reader->read_timeout_ms, true);
    atl06_quality_summary.join(info->reader->read_timeout_ms, true);
    segment_id.join(info->reader->read_timeout_ms, true);
    sigma_geo_h.join(info->reader->read_timeout_ms, true);
    x_atc.join(info->reader->read_timeout_ms, true);
    y_atc.join(info->reader->read_timeout_ms, true);
    seg_azimuth.join(info->reader->read_timeout_ms, true);
    dh_fit_dx.join(info->reader->read_timeout_ms, true);
    h_robust_sprd.join(info->reader->read_timeout_ms, true);
    n_fit_photons.join(info->reader->read_timeout_ms, true);
    w_surface_window_final.join(info->reader->read_timeout_ms, true);
    bsnow_conf.join(info->reader->read_timeout_ms, true);
    bsnow_h.join(info->reader->read_timeout_ms, true);
    r_eff.join(info->reader->read_timeout_ms, true);
    tide_ocean.join(info->reader->read_timeout_ms, true);

    /* Join Ancillary  Reads */
    if(anc_fields)
    {
        H5DArray* array = NULL;
        const char* dataset_name = anc_data.first(&array);
        while(dataset_name != NULL)
        {
            array->join(info->reader->read_timeout_ms, true);
            dataset_name = anc_data.next(&array);
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Atl06Data::~Atl06Data (void)
{
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl06Reader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Atl06Reader* reader = info->reader;
    Icesat2Parms* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};
    uint32_t extent_counter = 0;
    RecordObject** ancillary = NULL;
    long* ancillary_index = NULL;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "atl06_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", info->reader->asset->getName(), info->reader->resource, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        Region region(info);

        /* Read ATL06 Datasets */
        Atl06Data atl06(info, region);

        /* Create Elevation Batch Record */
        RecordObject atl06Batch(atRecType);
        atl06_t* record = reinterpret_cast<atl06_t*>(atl06Batch.getRecordData());
        int batch_index = 0;

        /* (Optionally) Create Ancillary Batch Record */
        if(parms->atl06_fields)
        {
            int num_anc_fields = parms->atl06_fields->length();
            if(num_anc_fields > 0)
            {
                ancillary = new RecordObject* [num_anc_fields];
                ancillary_index = new long [num_anc_fields];
                for(int i = 0; i < num_anc_fields; i++)
                {
                    ancillary[i] = new RecordObject(ancRecType);
                    ancillary_index[i] = 0;

                    /* Populate Field Indexes */
                    anc_t* rec = reinterpret_cast<anc_t*>(ancillary[i]->getRecordData());
                    rec->field_index = i;
                }
            }
        }

        /* Increment Read Statistics */
        local_stats.segments_read = region.latitude.size;

        /* Loop Through Each Segment */
        for(long segment = 0; reader->active && segment < region.num_segments; segment++)
        {
            /* Populate Elevation */
            elevation_t* entry = &record->elevation[batch_index++];
            entry->extent_id                = Icesat2Parms::generateExtentId(reader->start_rgt, reader->start_cycle, reader->start_region, info->track, info->pair, extent_counter) | Icesat2Parms::EXTENT_ID_ELEVATION;
            entry->time_ns                  = Icesat2Parms::deltatime2timestamp(atl06.delta_time[segment]);
            entry->segment_id               = atl06.segment_id[segment];
            entry->rgt                      = reader->start_rgt;
            entry->cycle                    = reader->start_cycle;
            entry->spot                     = Icesat2Parms::getSpotNumber((Icesat2Parms::sc_orient_t)atl06.sc_orient[0], (Icesat2Parms::track_t)info->track, info->pair);
            entry->gt                       = Icesat2Parms::getGroundTrack((Icesat2Parms::sc_orient_t)atl06.sc_orient[0], (Icesat2Parms::track_t)info->track, info->pair);
            entry->atl06_quality_summary    = atl06.atl06_quality_summary[segment];
            entry->bsnow_conf               = atl06.bsnow_conf[segment];
            entry->n_fit_photons            = atl06.n_fit_photons[segment];
            entry->latitude                 = region.latitude[segment];
            entry->longitude                = region.longitude[segment];
            entry->x_atc                    = atl06.x_atc[segment];
            entry->y_atc                    = atl06.y_atc[segment];
            entry->h_li                     = atl06.h_li[segment];
            entry->h_li_sigma               = atl06.h_li_sigma[segment];
            entry->sigma_geo_h              = atl06.sigma_geo_h[segment];
            entry->seg_azimuth              = atl06.seg_azimuth[segment];
            entry->dh_fit_dx                = atl06.dh_fit_dx[segment];
            entry->h_robust_sprd            = atl06.h_robust_sprd[segment];
            entry->w_surface_window_final   = atl06.w_surface_window_final[segment];
            entry->bsnow_h                  = atl06.bsnow_h[segment];
            entry->r_eff                    = atl06.r_eff[segment];
            entry->tide_ocean               = atl06.tide_ocean[segment];

            /* Populate Ancillary Data */
            if(parms->atl06_fields)
            {
                for(int i = 0; i < parms->atl06_fields->length(); i++)
                {
                    anc_t* rec = reinterpret_cast<anc_t*>(ancillary[i]->getRecordData());
                    rec->data[segment].extent_id = Icesat2Parms::generateExtentId(reader->start_rgt, reader->start_cycle, reader->start_region, info->track, info->pair, extent_counter) | Icesat2Parms::EXTENT_ID_ELEVATION;
                    const char* field_name = parms->atl06_fields->get(i).c_str();
                    rec->data_type = atl06.anc_data[field_name]->elementType();
                    atl06.anc_data[field_name]->serialize(&rec->data[segment].value[0], segment, 1);
                }
            }

            /* Post Records */
            if((batch_index == BATCH_SIZE) || (segment == (region.num_segments - 1)))
            {
                int post_status = MsgQ::STATE_TIMEOUT;
                unsigned char* buffer = NULL;
                int recsize = 0;
                int bufsize = 0;

                if(parms->atl06_fields)
                {
                    /* Create Container Record */
                    int max_con_rec_size = (sizeof(atl06_t) + sizeof(anc_t) + 256 /* record overhead */) * batch_index;
                    int num_recs = 1 + parms->atl06_fields->length();
                    ContainerRecord container(num_recs, max_con_rec_size);

                    /* Add Records to Container */
                    recsize += container.addRecord(atl06Batch, batch_index * sizeof(elevation_t));
                    for(int i = 0; i < parms->atl06_fields->length(); i++)
                    {
                        recsize += container.addRecord(*ancillary[i], offsetof(anc_t, data) + (batch_index * sizeof(anc_field_t)));
                    }

                    /* Serialize Container Record */
                    bufsize = container.serialize(&buffer, RecordObject::REFERENCE, recsize);

                }
                else // only an elevation record
                {
                    /* Serialize Elevation Batch Record */
                    recsize = batch_index * sizeof(elevation_t);
                    bufsize = atl06Batch.serialize(&buffer, RecordObject::REFERENCE, recsize);
                }

                /* Post Record */
                while(reader->active && (post_status = reader->outQ->postCopy(buffer, bufsize, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
                {
                    local_stats.extents_retried++;
                }

                /* Bumpst Statistics */
                if(post_status > 0)
                {
                    local_stats.extents_sent += batch_index;
                }
                else
                {
                    local_stats.extents_dropped += batch_index;
                }

                /* Reset Batch Index */
                batch_index = 0;
            }

            /* Bump Extent Counter */
            extent_counter++;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure during processing of resource %s track %d: %s", info->reader->resource, info->track, e.what());
        LuaEndpoint::generateExceptionStatus(e.code(), e.level(), reader->outQ, &reader->active, "%s: (%s)", e.what(), info->reader->resource);
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Update Statistics */
        reader->stats.segments_read += local_stats.segments_read;
        reader->stats.extents_filtered += local_stats.extents_filtered;
        reader->stats.extents_sent += local_stats.extents_sent;
        reader->stats.extents_dropped += local_stats.extents_dropped;
        reader->stats.extents_retried += local_stats.extents_retried;

        /* Count Completion */
        reader->numComplete++;
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", info->reader->resource);

            /* Indicate End of Data */
            if(reader->sendTerminator) reader->outQ->postCopy("", 0);
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up */
    delete info;
    if(parms->atl06_fields)
    {
        for(int i = 0; i < parms->atl06_fields->length(); i++)
        {
            delete ancillary[i];        
        }
        delete [] ancillary;
        delete [] ancillary_index;
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * parseResource
 *
 *  ATL0x_YYYYMMDDHHMMSS_ttttccrr_vvv_ee
 *      YYYY    - year
 *      MM      - month
 *      DD      - day
 *      HH      - hour
 *      MM      - minute
 *      SS      - second
 *      tttt    - reference ground track
 *      cc      - cycle
 *      rr      - region
 *      vvv     - version
 *      ee      - revision
 *----------------------------------------------------------------------------*/
void Atl06Reader::parseResource (const char* _resource, int32_t& rgt, int32_t& cycle, int32_t& region)
{
    if(StringLib::size(_resource) < 29)
    {
        rgt = 0;
        cycle = 0;
        region = 0;
        return; // early exit on error
    }

    long val;
    char rgt_str[5];
    rgt_str[0] = _resource[21];
    rgt_str[1] = _resource[22];
    rgt_str[2] = _resource[23];
    rgt_str[3] = _resource[24];
    rgt_str[4] = '\0';
    if(StringLib::str2long(rgt_str, &val, 10))
    {
        rgt = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse RGT from resource %s: %s", _resource, rgt_str);
    }

    char cycle_str[3];
    cycle_str[0] = _resource[25];
    cycle_str[1] = _resource[26];
    cycle_str[2] = '\0';
    if(StringLib::str2long(cycle_str, &val, 10))
    {
        cycle = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Cycle from resource %s: %s", _resource, cycle_str);
    }

    char region_str[3];
    region_str[0] = _resource[27];
    region_str[1] = _resource[28];
    region_str[2] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Region from resource %s: %s", _resource, region_str);
    }
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl06Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl06Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<Atl06Reader*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "read",        lua_obj->stats.segments_read);
        LuaEngine::setAttrInt(L, "filtered",    lua_obj->stats.extents_filtered);
        LuaEngine::setAttrInt(L, "sent",        lua_obj->stats.extents_sent);
        LuaEngine::setAttrInt(L, "dropped",     lua_obj->stats.extents_dropped);
        LuaEngine::setAttrInt(L, "retried",     lua_obj->stats.extents_retried);

        /* Clear if Requested */
        if(with_clear) memset(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
