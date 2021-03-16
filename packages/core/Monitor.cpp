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

#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Monitor::LuaMetaName = "Monitor";
const struct luaL_Reg Monitor::LuaMetaTable[] = {
    {"config",      luaConfig},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create([<type mask>], [<level>], [<output format>], [<outputq>])
 *----------------------------------------------------------------------------*/
int Monitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        uint8_t type_mask = (uint8_t)getLuaInteger(L, 1, true, (long)EventLib::LOG);
        event_level_t level = (event_level_t)getLuaInteger(L, 2, true, CRITICAL);
        format_t format = (format_t)getLuaInteger(L, 3, true, RECORD);
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return Dispatch Object */
        return createLuaObject(L, new Monitor(L, type_mask, level, format, outq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Monitor::Monitor(lua_State* L, uint8_t type_mask, event_level_t level, format_t format, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    /* Initialize Event Monitor */
    eventTypeMask = type_mask;
    eventLevel = level;
    outputFormat = format;

    /* Initialize Output Q */
    if(outq_name)   outQ = new Publisher(outq_name);
    else            outQ = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Monitor::~Monitor(void)
{
    if(outQ) delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Monitor::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    int event_size;
    char event_buffer[MAX_EVENT_SIZE];    

    /* Pull Out Log Message */
    EventLib::event_t* event = (EventLib::event_t*)record->getRecordData();

    /* Filter Events */
    if( ((event->type & eventTypeMask) == 0) ||
        (event->level < eventLevel) )
    {
        return true;
    }

    /* Process Event */
    if(outputFormat == RECORD)
    {
        /* Post Event as Record */
        unsigned char* buffer; // reference to serial buffer
        int size = record->serialize(&buffer, RecordObject::REFERENCE);
        if(outQ) outQ->postCopy(buffer, size, IO_CHECK);
    }
    else
    {
        /* Format Event */
        if(outputFormat == TEXT)
        {
            event_size = textOutput(event, event_buffer); 
        }
        else if(outputFormat == JSON)
        {
            event_size = jsonOutput(event, event_buffer); 
        }

        /* Post Event */
        if(outQ) outQ->postCopy(event_buffer, event_size, IO_CHECK);
        else     printf("%s", event_buffer);
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * textOutput
 *----------------------------------------------------------------------------*/
int Monitor::textOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Prefix */
    TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(event->systime);
    msg += StringLib::formats(msg, MAX_EVENT_SIZE, "%d:%d:%d:%d:%d:%s:%s ", 
        timeinfo.year, timeinfo.day, timeinfo.hour, timeinfo.minute, timeinfo.second,
        EventLib::lvl2str((event_level_t)event->level), event->name);

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_EVENT_SIZE - (msg - event_buffer), "%s\n", event->attr);

    /* Return Size of Message */
    return msg - event_buffer + 1;;
}

/*----------------------------------------------------------------------------
 * jsonOutput
 *----------------------------------------------------------------------------*/
int Monitor::jsonOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_EVENT_SIZE,
        "{\"systime\":%ld,\"ipv4\":%ld,\"flags\":%d,\"type\":%s,\"level:%s\",\"tid\":%ld,\"id\":%ld,\"parent\":%ld,\"name\":%s,\"attr\":%s}\n",
        event->systime, (long)event->ipv4, event->flags, 
        EventLib::type2str((EventLib::type_t)event->type), EventLib::lvl2str((event_level_t)event->level), 
        event->tid, (long)event->id, (long)event->parent, event->name, event->attr);

    /* Return Size of Message */
    return msg - event_buffer + 1;;
}

/*----------------------------------------------------------------------------
 * luaConfig - :config([<type mask>]) --> type mask, status
 *----------------------------------------------------------------------------*/
int Monitor::luaConfig (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Monitor* lua_obj = (Monitor*)getLuaSelf(L, 1);

        /* Set Type Mask */
        bool provided = false;
        uint8_t type_mask = getLuaInteger(L, 2, true, 0, &provided);
        if(provided) lua_obj->eventTypeMask = type_mask;

        /* Set Return Values */
        lua_pushinteger(L, (int)lua_obj->eventTypeMask);
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring monitor: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, 2);
}
