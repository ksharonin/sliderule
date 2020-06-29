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

#include "Atl03Device.h"
#include "GTArray.h"
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE           "srt"
#define LUA_PARM_SIGNAL_CONFIDENCE      "cnf"
#define LUA_PARM_ALONG_TRACK_SPREAD     "ats"
#define LUA_PARM_PHOTON_COUNT           "cnt"
#define LUA_PARM_EXTENT_LENGTH          "len"
#define LUA_PARM_EXTENT_STEP            "res"

#define LUA_STAT_SEGMENTS_READ_L        "read_l"
#define LUA_STAT_SEGMENTS_READ_R        "read_r"
#define LUA_STAT_EXTENTS_FILTERED_L     "filtered_l"
#define LUA_STAT_EXTENTS_FILTERED_R     "filtered_r"
#define LUA_STAT_EXTENTS_ADDED          "added"
#define LUA_STAT_EXTENTS_SENT           "sent"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Device::phRecType = "atl03rec.photons";
const RecordObject::fieldDef_t Atl03Device::phRecDef[] = {
    {"X",           RecordObject::DOUBLE,   offsetof(photon_t, distance_x), 1,  NULL, NATIVE_FLAGS},
    {"Y",           RecordObject::DOUBLE,   offsetof(photon_t, height_y),   1,  NULL, NATIVE_FLAGS}
};

const char* Atl03Device::exRecType = "atl03rec";
const RecordObject::fieldDef_t Atl03Device::exRecDef[] = {
    {"TRACK",       RecordObject::UINT8,    offsetof(extent_t, pair_reference_track),   1,  NULL, NATIVE_FLAGS},
    {"SEG_ID",      RecordObject::UINT32,   offsetof(extent_t, segment_id[0]),          2,  NULL, NATIVE_FLAGS},
    {"GPS",         RecordObject::DOUBLE,   offsetof(extent_t, gps_time[0]),            2,  NULL, NATIVE_FLAGS},
    {"DIST",        RecordObject::DOUBLE,   offsetof(extent_t, start_distance[0]),      2,  NULL, NATIVE_FLAGS},
    {"COUNT",       RecordObject::UINT32,   offsetof(extent_t, photon_count[0]),        2,  NULL, NATIVE_FLAGS},
    {"PHOTONS",     RecordObject::USER,     offsetof(extent_t, photon_offset[0]),       2,  phRecType, NATIVE_FLAGS | RecordObject::POINTER},
    {"DATA",        RecordObject::USER,     sizeof(extent_t),                           0,  phRecType, NATIVE_FLAGS} // variable length
};

const Atl03Device::parms_t Atl03Device::DefaultParms = {
    .surface_type = SRT_LAND_ICE,
    .signal_confidence = CNF_SURFACE_HIGH,
    .along_track_spread = 10.0, // meters
    .photon_count = 10, // PE
    .extent_length = 40.0, // meters
    .extent_step = 20.0 // meters
};

const double Atl03Device::ATL03_SEGMENT_LENGTH = 20.0; // meters
const double Atl03Device::MAX_ATL06_SEGMENT_LENGTH = 40.0; // meters

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<url>)
 *----------------------------------------------------------------------------*/
int Atl03Device::luaCreate (lua_State* L)
{
    try
    {
        /* Get URL */
        const char* url = getLuaString(L, 1);
        parms_t parms = DefaultParms;

        /* Check Config Table */
        if(lua_type(L, 2) == LUA_TTABLE)
        {
            bool provided = false;

            /* Get Configuration Parameters from Table */
            lua_getfield(L, 2, LUA_PARM_SURFACE_TYPE);
            parms.surface_type = (surfaceType_t)getLuaInteger(L, -1, true, parms.surface_type, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_SURFACE_TYPE, (int)parms.surface_type);

            lua_getfield(L, 2, LUA_PARM_SIGNAL_CONFIDENCE);
            parms.signal_confidence = (signalConf_t)getLuaInteger(L, -1, true, parms.signal_confidence, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_SIGNAL_CONFIDENCE, (int)parms.signal_confidence);

            lua_getfield(L, 2, LUA_PARM_ALONG_TRACK_SPREAD);
            parms.along_track_spread = getLuaFloat(L, -1, true, parms.along_track_spread, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_ALONG_TRACK_SPREAD, parms.along_track_spread);

            lua_getfield(L, 2, LUA_PARM_PHOTON_COUNT);
            parms.photon_count = getLuaInteger(L, -1, true, parms.photon_count, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_PHOTON_COUNT, parms.photon_count);

            lua_getfield(L, 2, LUA_PARM_EXTENT_LENGTH);
            parms.extent_length = getLuaFloat(L, -1, true, parms.extent_length, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_EXTENT_LENGTH, parms.extent_length);

            lua_getfield(L, 2, LUA_PARM_EXTENT_STEP);
            parms.extent_step = getLuaFloat(L, -1, true, parms.extent_step, &provided);
            if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_EXTENT_STEP, parms.extent_step);
        }

        /* Return Dispatch Object */
        return createLuaObject(L, new Atl03Device(L, url, parms));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating Atl03Device: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Device::init (void)
{
    RecordObject::recordDefErr_t ex_rc = RecordObject::defineRecord(exRecType, "TRACK", sizeof(extent_t), exRecDef, sizeof(exRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(ex_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", exRecType, ex_rc);
    }

    RecordObject::recordDefErr_t ph_rc = RecordObject::defineRecord(phRecType, NULL, sizeof(extent_t), phRecDef, sizeof(phRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(ph_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", phRecType, ph_rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Device::Atl03Device (lua_State* L, const char* url, parms_t _parms):
    DeviceObject(L, READER)
{
    /* Set Parameters */
    parms = _parms;

    /* Clear Statistics */
    LocalLib::set(&stats, 0, sizeof(stats));

    /* Set Segment List Index */
    listIndex = 0;

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s)", url, role == READER ? "READER" : "WRITER") + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s)", url, role == READER ? "READER" : "WRITER");

    /* Open URL */
    if(url[0])  connected = h5open(url);
    else        connected = false;

    /* Add Additional Meta Functions */
    LuaEngine::setAttrFunc(L, "parms",  luaParms);
    LuaEngine::setAttrFunc(L, "stats",  luaStats);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Device::~Atl03Device (void)
{
    if(config) delete [] config;

    for(int i = listIndex; i < extentList.length(); i++)
    {
        delete extentList[i];
    }
}

/*----------------------------------------------------------------------------
 * h5open
 *
 *  TODO: run this concurrent with readBuffer
 *  TODO: open and process all tracks in url
 *----------------------------------------------------------------------------*/
bool Atl03Device::h5open (const char* url)
{
    bool status = false;
    int track = 1; // hardcode for now

    try
    {
        /* Read Data from HDF5 File */
        H5Array<double>     sdp_gps_epoch   (url, "/ancillary_data/atlas_sdp_gps_epoch");
        GTArray<double>     delta_time      (url, track, "geolocation/delta_time");
        GTArray<int32_t>    segment_ph_cnt  (url, track, "geolocation/segment_ph_cnt");
        GTArray<int32_t>    segment_id      (url, track, "geolocation/segment_id");
        GTArray<double>     segment_dist_x  (url, track, "geolocation/segment_dist_x");
        GTArray<float>      dist_ph_along   (url, track, "heights/dist_ph_along");
        GTArray<float>      h_ph            (url, track, "heights/h_ph");
        GTArray<char>       signal_conf_ph  (url, track, "heights/signal_conf_ph", parms.surface_type);

        /* Initialize Dataset Scope Variables */
        int32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index
        int32_t seg_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // segment index
        int32_t seg_ph[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // current photon index in segment
        int32_t start_segment[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 };
        double  start_distance[PAIR_TRACKS_PER_GROUND_TRACK] = { segment_dist_x.gt[PRT_LEFT][0], segment_dist_x.gt[PRT_RIGHT][0] };
        bool    track_complete[PAIR_TRACKS_PER_GROUND_TRACK] = { false, false };

        /* Increment Read Statistics */
        stats.segments_read[PRT_LEFT] = segment_ph_cnt.gt[PRT_LEFT].size;
        stats.segments_read[PRT_RIGHT] = segment_ph_cnt.gt[PRT_RIGHT].size;

        /* Traverse All Photons In Dataset */
        while( !track_complete[PRT_LEFT] || !track_complete[PRT_RIGHT] )
        {
            List<photon_t> extent_photons[PAIR_TRACKS_PER_GROUND_TRACK];
            int32_t extent_segment[PAIR_TRACKS_PER_GROUND_TRACK];
            bool extent_valid[PAIR_TRACKS_PER_GROUND_TRACK] = { true, true };

            /* Select Photons for Extent from each Track */
            for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
            {
                int32_t current_photon = ph_in[t];
                int32_t current_segment = seg_in[t];
                int32_t current_count = seg_ph[t]; // number of photons in current segment already accounted for
                bool extent_complete = false;
                bool step_complete = false;

                /* Set Extent Segment */
                extent_segment[t] = seg_in[t];

                /* Traverse Photons Until Desired Along Track Distance Reached */
                while( (!extent_complete || !step_complete) &&              // extent or step not complete
                        (current_segment < segment_dist_x.gt[t].size) &&     // there are more segments
                        (current_photon < dist_ph_along.gt[t].size) )        // there are more photons
                {
                    /* Go to Photon's Segment */
                    current_count++;
                    while(current_count > segment_ph_cnt.gt[t][current_segment])
                    {
                        current_count = 1; // reset photons in segment
                        current_segment++; // go to next segment
                        if(current_segment >= segment_dist_x.gt[t].size)
                        {
                            break;
                        }
                    }

                    /* Update Along Track Distance */
                    double delta_distance = segment_dist_x.gt[t][current_segment] - start_distance[t];
                    double along_track_distance = delta_distance + dist_ph_along.gt[t][current_photon];

                    /* Set Next Extent's First Photon */
                    if(!step_complete && along_track_distance >= parms.extent_step)
                    {
                        ph_in[t] = current_photon;
                        seg_in[t] = current_segment;
                        seg_ph[t] = current_count - 1;
                        step_complete = true;
                    }

                    /* Check if Photon within Extent's Length */
                    if(along_track_distance < parms.extent_length)
                    {
                        /* Check  Photon Signal Confidence Level */
                        if(signal_conf_ph.gt[t][current_photon] >= parms.signal_confidence)
                        {
                            photon_t ph = {
                                .distance_x = delta_distance + dist_ph_along.gt[t][current_photon],
                                .height_y = h_ph.gt[t][current_photon]
                            };
                            extent_photons[t].add(ph);
                        }
                    }
                    else if(!extent_complete)
                    {
                        extent_complete = true;
                    }

                    /* Go to Next Photon */
                    current_photon++;
                }

                /* Add Step to Start Distance */
                start_distance[t] += parms.extent_step;

                /* Apply Segment Distance Correction and Update Start Segment */
                while( ((start_segment[t] + 1) < segment_dist_x.gt[t].size) &&
                        (start_distance[t] >= segment_dist_x.gt[t][start_segment[t] + 1]) )
                {
                    start_distance[t] += segment_dist_x.gt[t][start_segment[t] + 1] - segment_dist_x.gt[t][start_segment[t]];
                    start_distance[t] -= ATL03_SEGMENT_LENGTH;
                    start_segment[t]++;
                }

                /* Check if Track Complete */
                if(current_photon >= dist_ph_along.gt[t].size)
                {
                    track_complete[t] = true;
                }

                /* Check Photon Count */
                if(extent_photons[t].length() < parms.photon_count)
                {
                    extent_valid[t] = false;
                }

                /* Check Along Track Spread */
                if(extent_photons[t].length() > 1)
                {
                    int32_t last = extent_photons[t].length() - 1;
                    double along_track_spread = extent_photons[t][last].distance_x - extent_photons[t][0].distance_x;
                    if(along_track_spread < parms.along_track_spread)
                    {
                        extent_valid[t] = false;
                    }
                }

                /* Incrment Statistics if Invalid */
                if(!extent_valid[t])
                {
                    stats.extents_filtered[t]++;
                }
            }

            /* Create Extent Record */
            if(extent_valid[PRT_LEFT] || extent_valid[PRT_RIGHT])
            {
                /* Calculate Extent Record Size */
                int extent_size = sizeof(extent_t) + (sizeof(photon_t) * (extent_photons[PRT_LEFT].length() + extent_photons[PRT_RIGHT].length()));

                /* Allocate and Initialize Extent Record */
                RecordObject* record = new RecordObject(exRecType, extent_size); // overallocated memory... photons filtered below
                extent_t* extent = (extent_t*)record->getRecordData();
                extent->pair_reference_track = track;

                /* Populate Extent */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Populate Attributes */
                    extent->segment_id[t] = segment_id.gt[t][extent_segment[t]];
                    extent->gps_time[t] = sdp_gps_epoch[0] + delta_time.gt[t][extent_segment[t]];
                    extent->start_distance[t] = segment_dist_x.gt[t][extent_segment[t]];
                    extent->photon_count[t] = extent_photons[t].length();

                    /* Populate Photons */
                    for(int32_t p = 0; p < extent_photons[t].length(); p++)
                    {
                        extent->photons[ph_out++] = extent_photons[t][p];
                    }
                }

                /* Set Photon Pointer Fields */
                extent->photon_offset[PRT_LEFT] = sizeof(extent_t); // pointers are set to offset from start of record data
                extent->photon_offset[PRT_RIGHT] = sizeof(extent_t) + (sizeof(photon_t) * extent->photon_count[PRT_LEFT]);

                /* Add Segment Record */
                extentList.add(record);
                stats.extents_added++;
            }
        }

        /* Set Success */
        status = true;
    }
    catch(const std::exception& e)
    {
        mlog(CRITICAL, "Unable to process resource %s: %s\n", url, e.what());
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool Atl03Device::isConnected (int num_open)
{
    (void)num_open;

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Atl03Device::closeConnection (void)
{
    connected = false;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Atl03Device::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Atl03Device::readBuffer (void* buf, int len)
{
    int bytes = TIMEOUT_RC;

    if(connected)
    {
        unsigned char* rec_buf = (unsigned char*)buf;

        /* Read Next Segment in List */
        if(listIndex < extentList.length())
        {
            RecordObject* record = extentList[listIndex];

            /* Check if Enough Room in Buffer to Hold Record */
            if(len >= record->getAllocatedMemory())
            {
                /* Serialize Record into Buffer */
                bytes = record->serialize(&rec_buf, RecordObject::COPY, len);
                stats.extents_sent++;
            }
            else
            {
                mlog(ERROR, "Unable to read ATL03 extent record, buffer too small (%d < %d)\n", len, record->getAllocatedMemory());
            }

            /* Release Segment Resources */
            delete record;

            /* Go To Next Segment */
            listIndex++;
        }
        else
        {
            bytes = SHUTDOWN_RC;
            connected = false;
        }
    }

    return bytes;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Atl03Device::getUniqueId (void)
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* Atl03Device::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Atl03Device::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Device* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Device*)getLuaSelf(L, 1);
    }
    catch(const LuaException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_PARM_SURFACE_TYPE,         lua_obj->parms.surface_type);
        LuaEngine::setAttrInt(L, LUA_PARM_SIGNAL_CONFIDENCE,    lua_obj->parms.signal_confidence);
        LuaEngine::setAttrNum(L, LUA_PARM_ALONG_TRACK_SPREAD,   lua_obj->parms.along_track_spread);
        LuaEngine::setAttrInt(L, LUA_PARM_PHOTON_COUNT,         lua_obj->parms.photon_count);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_LENGTH,        lua_obj->parms.extent_length);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_STEP,          lua_obj->parms.extent_step);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning parameters %s: %s\n", lua_obj->getName(), e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl03Device::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Device* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Device*)getLuaSelf(L, 1);
    }
    catch(const LuaException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_L,      lua_obj->stats.segments_read[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ_R,      lua_obj->stats.segments_read[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED_L,  lua_obj->stats.extents_filtered[PRT_LEFT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED_R,  lua_obj->stats.extents_filtered[PRT_RIGHT]);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_ADDED,        lua_obj->stats.extents_added);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_SENT,        lua_obj->stats.extents_sent);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error returning stats %s: %s\n", lua_obj->getName(), e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
