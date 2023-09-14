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

#include "core.h"
#include "geo.h"
#include "RasterSampler.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RasterSampler::LuaMetaName = "RasterSampler";
const struct luaL_Reg RasterSampler::LuaMetaTable[] = {
    {NULL,          NULL}
};

const char* RasterSampler::rsSampleRecType = "rsrec.sample";
const RecordObject::fieldDef_t RasterSampler::rsSampleRecDef[] = {
    {"value",           RecordObject::DOUBLE,   offsetof(sample_t, value),      1,  NULL, NATIVE_FLAGS},
    {"time",            RecordObject::DOUBLE,   offsetof(sample_t, time),       1,  NULL, NATIVE_FLAGS},
    {"file_id",         RecordObject::UINT64,   offsetof(sample_t, file_id),    1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT32,   offsetof(sample_t, flags),      1,  NULL, NATIVE_FLAGS}
};

const char* RasterSampler::rsGeoRecType = "rsrec";
const RecordObject::fieldDef_t RasterSampler::rsGeoRecDef[] = {
    {"index",           RecordObject::UINT64,   offsetof(rs_geo_t, index),           1,  NULL, NATIVE_FLAGS},
    {"key",             RecordObject::STRING,   offsetof(rs_geo_t, raster_key),      RASTER_KEY_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"num_samples",     RecordObject::UINT32,   offsetof(rs_geo_t, num_samples),     1,  NULL, NATIVE_FLAGS},
    {"samples",         RecordObject::USER,     offsetof(rs_geo_t, samples),         0,  rsSampleRecType, NATIVE_FLAGS} // variable length
};

const char* RasterSampler::zsSampleRecType = "zsrec.sample";
const RecordObject::fieldDef_t RasterSampler::zsSampleRecDef[] = {
    {"value",           RecordObject::DOUBLE,   offsetof(RasterSample, value),        1,  NULL, NATIVE_FLAGS},
    {"time",            RecordObject::DOUBLE,   offsetof(RasterSample, time),         1,  NULL, NATIVE_FLAGS},
    {"file_id",         RecordObject::UINT64,   offsetof(RasterSample, fileId),       1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT32,   offsetof(RasterSample, flags),        1,  NULL, NATIVE_FLAGS},
    {"count",           RecordObject::UINT32,   offsetof(RasterSample, stats.count),  1,  NULL, NATIVE_FLAGS},
    {"min",             RecordObject::DOUBLE,   offsetof(RasterSample, stats.min),    1,  NULL, NATIVE_FLAGS},
    {"max",             RecordObject::DOUBLE,   offsetof(RasterSample, stats.max),    1,  NULL, NATIVE_FLAGS},
    {"mean",            RecordObject::DOUBLE,   offsetof(RasterSample, stats.mean),   1,  NULL, NATIVE_FLAGS},
    {"median",          RecordObject::DOUBLE,   offsetof(RasterSample, stats.median), 1,  NULL, NATIVE_FLAGS},
    {"stdev",           RecordObject::DOUBLE,   offsetof(RasterSample, stats.stdev),  1,  NULL, NATIVE_FLAGS},
    {"mad",             RecordObject::DOUBLE,   offsetof(RasterSample, stats.mad),    1,  NULL, NATIVE_FLAGS}
};

const char* RasterSampler::zsGeoRecType = "zsrec";
const RecordObject::fieldDef_t RasterSampler::zsGeoRecDef[] = {
    {"index",           RecordObject::UINT64,   offsetof(zs_geo_t, index),           1,  NULL, NATIVE_FLAGS},
    {"key",             RecordObject::STRING,   offsetof(zs_geo_t, raster_key),      RASTER_KEY_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"num_samples",     RecordObject::UINT32,   offsetof(zs_geo_t, num_samples),     1,  NULL, NATIVE_FLAGS},
    {"samples",         RecordObject::USER,     offsetof(zs_geo_t, samples),         0,  zsSampleRecType, NATIVE_FLAGS} // variable length
};

const char* RasterSampler::fileIdRecType = "fileidrec";
const RecordObject::fieldDef_t RasterSampler::fileIdRecDef[] = {
    {"file_id",         RecordObject::UINT64,   offsetof(file_directory_entry_t, file_id),      1,  NULL, NATIVE_FLAGS},
    {"file_name",       RecordObject::STRING,   offsetof(file_directory_entry_t, file_name),    0,  NULL, NATIVE_FLAGS} // variable length
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :sampler(<raster>, <raster_key>, <outq name>, <rec_type>, <index_key>, <lon_key>, <lat_key>)
 *----------------------------------------------------------------------------*/
int RasterSampler::luaCreate (lua_State* L)
{
    RasterObject* _raster = NULL;
    try
    {
        /* Get Parameters */
        _raster                 = (RasterObject*)getLuaObject(L, 1, RasterObject::OBJECT_TYPE);
        const char* raster_key  = getLuaString(L, 2);
        const char* outq_name   = getLuaString(L, 3);
        const char* rec_type    = getLuaString(L, 4);
        const char* index_key   = getLuaString(L, 5);
        const char* lon_key     = getLuaString(L, 6);
        const char* lat_key     = getLuaString(L, 7);
        const char* time_key    = getLuaString(L, 8, true, NULL);
        const char* height_key  = getLuaString(L, 9, true, NULL);

        /* Create Dispatch */
        return createLuaObject(L, new RasterSampler(L, _raster, raster_key, outq_name, rec_type, index_key, lon_key, lat_key, time_key, height_key));
    }
    catch(const RunTimeException& e)
    {
        if(_raster) _raster->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void RasterSampler::init (void)
{
    RECDEF(rsSampleRecType, rsSampleRecDef, sizeof(sample_t), NULL);
    RECDEF(rsGeoRecType,    rsGeoRecDef,    sizeof(rs_geo_t), NULL);
    RECDEF(zsSampleRecType, zsSampleRecDef, sizeof(RasterSample), NULL);
    RECDEF(zsGeoRecType,    zsGeoRecDef,    sizeof(zs_geo_t), NULL);
    RECDEF(fileIdRecType,   fileIdRecDef,   sizeof(file_directory_entry_t), NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void RasterSampler::deinit (void)
{
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterSampler::RasterSampler (lua_State* L, RasterObject* _raster, const char* raster_key,
                              const char* outq_name, const char* rec_type,
                              const char* index_key, const char* lon_key, const char* lat_key,
                              const char* time_key, const char* height_key):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(_raster);
    assert(outq_name);
    assert(lon_key);
    assert(lat_key);

    /* Initialize Class Data */
    raster = _raster;
    rasterKey = StringLib::duplicate(raster_key);
    outQ = new Publisher(outq_name);

    /* Determine Record Batch Size */
    batchRecordSizeBytes = 0;
    Dictionary<RecordObject::field_t>* fields = RecordObject::getRecordFields(rec_type);
    Dictionary<RecordObject::field_t>::Iterator field_iter(*fields);
    for(int i = 0; i < field_iter.length; i++)
    {
        if(field_iter[i].value.flags & RecordObject::BATCH)
        {
            batchRecordSizeBytes = RecordObject::getRecordDataSize(field_iter[i].value.exttype);
            break;
        }
    }

    /* Determine Record Size */
    recordSizeBytes = RecordObject::getRecordDataSize(rec_type) + batchRecordSizeBytes;
    if(recordSizeBytes <= 0)
    {
        mlog(CRITICAL, "Failed to get size of record: %s", rec_type);
    }

    /* Get Index Field (e.g. Extent Id) */
    indexField = RecordObject::getDefinedField(rec_type, index_key);
    if(indexField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", index_key, rec_type);
    }

    /* Get Longitude Field */
    lonField = RecordObject::getDefinedField(rec_type, lon_key);
    if(lonField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", lon_key, rec_type);
    }

    /* Get Latitude Field */
    latField = RecordObject::getDefinedField(rec_type, lat_key);
    if(latField.type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Failed to get field %s from record type: %s", lat_key, rec_type);
    }

    /* Get Time Field */
    timeField.type = RecordObject::INVALID_FIELD;
    if(time_key)
    {
        timeField = RecordObject::getDefinedField(rec_type, time_key);
        if(timeField.type == RecordObject::INVALID_FIELD)
        {
            mlog(CRITICAL, "Failed to get field %s from record type: %s", time_key, rec_type);
        }
    }

    /* Get Height Field */
    heightField.type = RecordObject::INVALID_FIELD;
    if(height_key)
    {
        heightField = RecordObject::getDefinedField(rec_type, height_key);
        if(heightField.type == RecordObject::INVALID_FIELD)
        {
            mlog(CRITICAL, "Failed to get field %s from record type: %s", height_key, rec_type);
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
RasterSampler::~RasterSampler(void)
{
    raster->releaseLuaObject();
    delete outQ;
    if(rasterKey) delete [] rasterKey;
}

/*----------------------------------------------------------------------------
 * processRecord
 *
 *  OUTPUT: one rs_geo_t record per index
 *  INPUT:  batch of atl06 extents
 *          each extent (up to 256 per record) will produce a single output record with one point
 *          that one point may have multiple samples associated with it
 *----------------------------------------------------------------------------*/
bool RasterSampler::processRecord (RecordObject* record, okey_t key, recVec_t* records)
{
    (void)key;
    (void)records;

    bool status = true;

    /* Determine Number of Rows in Record */
    int record_size_bytes = record->getAllocatedDataSize();
    int batch_size_bytes = record_size_bytes - (recordSizeBytes - batchRecordSizeBytes);
    int num_batches = 1;
    if(batch_size_bytes > 0)
    {
        num_batches = batch_size_bytes / batchRecordSizeBytes;
        int left_over = batch_size_bytes % batchRecordSizeBytes;
        if(left_over > 0)
        {
            mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", record->getRecordType(), batch_size_bytes, batchRecordSizeBytes);
            return false;
        }
    }

    /* Initialize Local Fields */
    RecordObject::field_t index_field = indexField;
    RecordObject::field_t lon_field = lonField;
    RecordObject::field_t lat_field = latField;
    RecordObject::field_t time_field = timeField;
    RecordObject::field_t height_field = heightField;

    /* Loop Through Each Record in Batch */
    for(int batch = 0; batch < num_batches; batch++)
    {
        /* Get Index (e.g. Extent Id) */
        uint64_t index = (uint64_t)record->getValueInteger(index_field);
        index_field.offset += (batchRecordSizeBytes * 8);

        /* Get Longitude */
        double lon_val = record->getValueReal(lon_field);
        lon_field.offset += (batchRecordSizeBytes * 8);

        /* Get Latitude */
        double lat_val = record->getValueReal(lat_field);
        lat_field.offset += (batchRecordSizeBytes * 8);

        /* Get Time */
        long gps = 0;
        if(time_field.type != RecordObject::INVALID_FIELD)
        {
            long time_val = record->getValueInteger(time_field);
            time_field.offset += (batchRecordSizeBytes * 8);
            gps = TimeLib::sysex2gpstime(time_val);
        }

        /* Get Height */
        double height_val = 0.0;
        if(height_field.type != RecordObject::INVALID_FIELD)
        {
            height_val = record->getValueReal(height_field);
            height_field.offset += (batchRecordSizeBytes * 8);
        }

        /* Sample Raster */
        std::vector<RasterSample> slist;
        OGRPoint poi(lon_val, lat_val, height_val);
        int num_samples = 0;
        try
        {
            raster->getSamples(&poi, gps, slist);
            num_samples = slist.size();
        }
        catch(const RunTimeException& e)
        {
            LuaEndpoint::generateExceptionStatus(RTE_ERROR, e.level(), outQ, NULL,
                                                "Exception caught when sampling %s at %.3lf,%.3lf,%3lf: %s",
                                                rasterKey, lon_val, lat_val, height_val, e.what());
        }

        if(raster->hasZonalStats())
        {
            /* Create and Post Sample Record */
            int size_of_record = offsetof(zs_geo_t, samples) + (sizeof(RasterSample) * num_samples);
            RecordObject stats_rec(zsGeoRecType, size_of_record);
            zs_geo_t* data = (zs_geo_t*)stats_rec.getRecordData();
            data->index = index;
            StringLib::copy(data->raster_key, rasterKey, RASTER_KEY_MAX_LEN);
            data->num_samples = num_samples;
            for(int i = 0; i < num_samples; i++)
            {
                data->samples[i] = slist[i];
            }
            if(!stats_rec.post(outQ))
            {
                status = false;
            }
        }
        else
        {
            /* Create and Post Sample Record */
            int size_of_record = offsetof(rs_geo_t, samples) + (sizeof(sample_t) * num_samples);
            RecordObject sample_rec(rsGeoRecType, size_of_record);
            rs_geo_t* data = (rs_geo_t*)sample_rec.getRecordData();
            data->index = index;
            StringLib::copy(data->raster_key, rasterKey, RASTER_KEY_MAX_LEN);
            data->num_samples = num_samples;
            for(int i = 0; i < num_samples; i++)
            {
                data->samples[i].value = slist[i].value;
                data->samples[i].time = slist[i].time;
                data->samples[i].file_id = slist[i].fileId;
                data->samples[i].flags = slist[i].flags;
            }
            if(!sample_rec.post(outQ))
            {
                status = false;
            }
        }
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool RasterSampler::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool RasterSampler::processTermination (void)
{
    Dictionary<uint64_t>::Iterator iterator(raster->fileDictGet());
    for(int i = 0; i < iterator.length; i++)
    {
        /* Send File Directory Entry Record for each File in Raster Dictionary */
        int file_name_len = StringLib::size(iterator[i].key) + 1;
        int size = offsetof(file_directory_entry_t, file_name) + file_name_len;
        RecordObject record(fileIdRecType, size);
        file_directory_entry_t* entry = (file_directory_entry_t*)record.getRecordData();
        entry->file_id = iterator[i].value;
        StringLib::copy(entry->file_name, iterator[i].key, file_name_len);
        record.post(outQ);
    }
    return true;
}
