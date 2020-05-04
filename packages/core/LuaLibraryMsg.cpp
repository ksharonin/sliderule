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

#include "LuaLibraryMsg.h"
#include "core.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibraryMsg::LUA_MSGLIBNAME   = "msg";
const char* LuaLibraryMsg::LUA_PUBMETANAME  = "LuaLibraryMsg.publisher";
const char* LuaLibraryMsg::LUA_SUBMETANAME  = "LuaLibraryMsg.subscriber";
const char* LuaLibraryMsg::LUA_RECMETANAME  = "LuaLibraryMsg.record";

const struct luaL_Reg LuaLibraryMsg::msgLibsF [] = {
    {"publish",       LuaLibraryMsg::lmsg_publish},
    {"subscribe",     LuaLibraryMsg::lmsg_subscribe},
    {"create",        LuaLibraryMsg::lmsg_create},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::pubLibsM [] = {
    {"sendstring",    LuaLibraryMsg::lmsg_sendstring},
    {"sendrecord",    LuaLibraryMsg::lmsg_sendrecord},
    {"__gc",          LuaLibraryMsg::lmsg_deletepub},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::subLibsM [] = {
    {"recvstring",    LuaLibraryMsg::lmsg_recvstring},
    {"recvrecord",    LuaLibraryMsg::lmsg_recvrecord},
    {"drain",         LuaLibraryMsg::lmsg_drain},
    {"__gc",          LuaLibraryMsg::lmsg_deletesub},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::recLibsM [] = {
    {"gettype",       LuaLibraryMsg::lmsg_gettype},
    {"getvalue",      LuaLibraryMsg::lmsg_getfieldvalue},
    {"setvalue",      LuaLibraryMsg::lmsg_setfieldvalue},
    {"serialize",     LuaLibraryMsg::lmsg_serialize},
    {"deserialize",   LuaLibraryMsg::lmsg_deserialize},
    {"__gc",          LuaLibraryMsg::lmsg_deleterec},
    {NULL, NULL}
};

LuaLibraryMsg::createRecFunc LuaLibraryMsg::prefixLookUp[0xFF];
Dictionary<LuaLibraryMsg::associateRecFunc> LuaLibraryMsg::typeLookUp;

/******************************************************************************
 * LIBRARY METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_init
 *----------------------------------------------------------------------------*/
void LuaLibraryMsg::lmsg_init (void)
{
    LocalLib::set(prefixLookUp, 0, sizeof(prefixLookUp));
}

/*----------------------------------------------------------------------------
 * lmsg_addtype
 *----------------------------------------------------------------------------*/
bool LuaLibraryMsg::lmsg_addtype (char prefix, createRecFunc cfunc, const char* recclass, associateRecFunc afunc)
{
    if(prefix != '\0')
    {
        prefixLookUp[(uint8_t)prefix] = cfunc;
    }

    if(recclass != NULL)
    {
        typeLookUp.add(recclass, afunc);
    }

    return true;
}

/*----------------------------------------------------------------------------
 * luaopen_msglib
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::luaopen_msglib (lua_State *L)
{
    /* Setup Publisher */
    luaL_newmetatable(L, LUA_PUBMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, pubLibsM, 0);

    /* Setup Subscriber */
    luaL_newmetatable(L, LUA_SUBMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, subLibsM, 0);

    /* Setup Record */
    luaL_newmetatable(L, LUA_RECMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, recLibsM, 0);

    /* Setup Functions */
    luaL_newlib(L, msgLibsF);

    return 1;
}

/*----------------------------------------------------------------------------
 * populateRecord
 *----------------------------------------------------------------------------*/
RecordObject* LuaLibraryMsg::populateRecord (const char* population_string)
{
    /* Create record */
    RecordObject* record = NULL;
    try
    {
        uint8_t class_prefix = population_string[0];
        createRecFunc func = prefixLookUp[class_prefix];
        if(func != NULL)    record = func(&population_string[1]); // skip prefix
        else                record = new RecordObject(population_string);
    }
    catch (const InvalidRecordException& e)
    {
        if(record) delete record;
        mlog(ERROR, "could not locate record definition for %s: %s\n", population_string, e.what());
    }

    return record;
}

/*----------------------------------------------------------------------------
 * associateRecord
 *----------------------------------------------------------------------------*/
RecordObject* LuaLibraryMsg::associateRecord (const char* record_class, unsigned char* data, int size)
{
    /* Create record */
    RecordObject* record = NULL;
    try
    {
        if(record_class)
        {
            associateRecFunc func = typeLookUp[record_class];
            record = func(data, size);
        }
        else
        {
            record = new RecordObject(data, size);
        }
    }
    catch (const InvalidRecordException& e)
    {
        if(record) delete record;
        mlog(ERROR, "could not locate record definition for %s: %s\n", record_class, e.what());
    }

    return record;
}

/******************************************************************************
 * MESSAGE QUEUE LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_publish - var = msg.publish(<msgq name>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_publish (lua_State* L)
{
    /* Get Message Queue Name */
    const char* msgq_name = lua_tostring(L, 1);

    /* Get Message User Data */
    msgPublisherData_t* msg_data = (msgPublisherData_t*)lua_newuserdata(L, sizeof(msgPublisherData_t));
    msg_data->msgq_name = StringLib::duplicate(msgq_name);
    msg_data->pub = new Publisher(msgq_name);

    /* Return msgUserData_t to Lua */
    luaL_getmetatable(L, LUA_PUBMETANAME);
    lua_setmetatable(L, -2);    /* associates the publisher meta table with the publisher user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_subscribe - var = msg.subscribe(<msgq name>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_subscribe (lua_State* L)
{
    /* Get Message Queue Name */
    const char* msgq_name = lua_tostring(L, 1);

    /* Get Message User Data */
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)lua_newuserdata(L, sizeof(msgSubscriberData_t));
    msg_data->msgq_name = StringLib::duplicate(msgq_name);
    msg_data->sub = new Subscriber(msgq_name);

    /* Return msgUserData_t to Lua */
    luaL_getmetatable(L, LUA_SUBMETANAME);
    lua_setmetatable(L, -2);    /* associates the subscriber meta table with the subscriber user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_create -
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_create (lua_State* L)
{
    /* Create Record */
    const char* population_string = lua_tostring(L, 1);
    RecordObject* record = populateRecord(population_string);
    if(record == NULL)
    {
        return luaL_error(L, "invalid record specified");
    }

    /* Create User Data */
    recUserData_t* rec_data = (recUserData_t*)lua_newuserdata(L, sizeof(recUserData_t));
    rec_data->record_str = StringLib::duplicate(population_string);
    rec_data->rec = record;

    /* Return recUserData_t to Lua */
    luaL_getmetatable(L, LUA_RECMETANAME);
    lua_setmetatable(L, -2);    /* associates the publisher meta table with the publisher user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_sendstring
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_sendstring (lua_State* L)
{
    size_t len;

    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    const char* str = lua_tolstring(L, 2, &len);        /* get argument */
    int status = msg_data->pub->postCopy(str, len);     /* post string */
    lua_pushboolean(L, status > 0);                     /* push result status */
    return 1;                                           /* number of results */
}

/*----------------------------------------------------------------------------
 * lmsg_sendrecord - <record | population string>
 *
 *  Note that there are different types of records.  The BASE class record can be sent
 *  without any explicit specification.  Other record types require a special character
 *  to be prefixed to the population string.  Currently the following record
 *  types are explicitly supported:
 *      BASE  - <population string>
 *      CCSDS - /<population string>
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_sendrecord (lua_State* L)
{
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    RecordObject* record = NULL;
    bool allocated = false;

    if(lua_isuserdata(L, 2)) // user data record
    {
        LuaLibraryMsg::recUserData_t* rec_data = (LuaLibraryMsg::recUserData_t*)luaL_checkudata(L, 2, LuaLibraryMsg::LUA_RECMETANAME);
        record = rec_data->rec;
        if(record == NULL)
        {
            return luaL_error(L, "nill record supplied.");
        }
    }
    else // population string
    {
        const char* population_string = lua_tostring(L, 2);  /* get argument */
        record = populateRecord(population_string);
        allocated = true;
        if(record == NULL)
        {
            return luaL_error(L, "invalid record retrieved.");
        }
    }

    /* Post Serialized Record */
    int status = 0; // initial status returned is false
    unsigned char* buffer; // reference to serial buffer
    int size = record->serialize(&buffer, RecordObject::REFERENCE);
    if(size > 0) status = msg_data->pub->postCopy(buffer, size);
    if(status <= 0) // if size check fails above, then status will remain zero
    {
        mlog(CRITICAL, "Failed to post record %s to %s with error code %d\n", record->getRecordType(), msg_data->pub->getName(), status);
    }

    /* Clean Up and Return Status */
    if(allocated) delete record;
    lua_pushboolean(L, status > 0);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_deletepub
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deletepub (lua_State* L)
{
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    if(msg_data->msgq_name) delete [] msg_data->msgq_name;
    if(msg_data->pub) delete msg_data->pub;
    return 0;
}

/*----------------------------------------------------------------------------
 * lmsg_recvstring
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_recvstring (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);
    int timeoutms = (int)lua_tointeger(L, 2);
    char str[MAX_STR_SIZE];
    int strlen = msg_data->sub->receiveCopy(str, MAX_STR_SIZE - 1, timeoutms);
    if(strlen > 0)
    {
        lua_pushlstring(L, str, strlen);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_recvrecord
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_recvrecord (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);
    int timeoutms = (int)lua_tointeger(L, 2);
    const char* rec_class = NULL;
    if(lua_isstring(L, 3))
    {
        rec_class = (const char*)lua_tostring(L, 3);
    }

    Subscriber::msgRef_t ref;
    int status = msg_data->sub->receiveRef(ref, timeoutms);
    if(status > 0)
    {
        /* Get Record */
        RecordObject* record = associateRecord(rec_class, (unsigned char*)ref.data, ref.size);

        /* Free Reference */
        msg_data->sub->dereference(ref);

        /* Assign Record to User Data */
        if(record)
        {
            LuaLibraryMsg::recUserData_t* rec_data = (LuaLibraryMsg::recUserData_t*)lua_newuserdata(L, sizeof(LuaLibraryMsg::recUserData_t));
            rec_data->record_str = NULL;
            rec_data->rec = record;
            luaL_getmetatable(L, LUA_RECMETANAME);
            lua_setmetatable(L, -2); // associates the record meta table with the record user data
            return 1;
        }
        else
        {
            mlog(WARNING, "Unable to create record object: %s\n", rec_class);
        }
    }
    else if(status != MsgQ::STATE_TIMEOUT)
    {
        mlog(CRITICAL, "Failed (%d) to receive record on message queue %s", status, msg_data->sub->getName());
    }

    /* Failed to receive record */
    lua_pushnil(L);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_drain
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_drain (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);

    msg_data->sub->drain();

    lua_pushboolean(L, true);

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_deletesub
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deletesub (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);
    if(msg_data->msgq_name) delete [] msg_data->msgq_name;
    if(msg_data->sub) delete msg_data->sub;
    return 0;
}

/******************************************************************************
 * RECORD LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_gettype - val = rec:gettype()
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_gettype (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    const char* rectype = rec_data->rec->getRecordType();
    lua_pushstring(L, rectype);

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_getfieldvalue - val = rec:getfieldvalue(<field name>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_getfieldvalue (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    const char* fldname = lua_tostring(L, 2); // get field name

    if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    RecordObject::field_t field = rec_data->rec->getField(fldname);
    RecordObject::valType_t valtype = rec_data->rec->getValueType(field);
    if(valtype == RecordObject::TEXT)
    {
        char valbuf[RecordObject::MAX_VAL_STR_SIZE];
        const char* val = rec_data->rec->getValueText(field, valbuf);
        lua_pushstring(L, val);
    }
    else if(valtype == RecordObject::REAL)
    {
        double val = rec_data->rec->getValueReal(field);
        lua_pushnumber(L, val);
    }
    else if(valtype == RecordObject::INTEGER)
    {
        long val = rec_data->rec->getValueInteger(field);
        lua_pushnumber(L, (double)val);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_setfieldvalue - rec:setfieldvalue(<field name>, <value>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_setfieldvalue (lua_State* L)
{
    bool status = true;
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    const char* fldname = lua_tostring(L, 2);  /* get field name */

    if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    RecordObject::field_t field = rec_data->rec->getField(fldname);
    RecordObject::valType_t valtype = rec_data->rec->getValueType(field);
    if(valtype == RecordObject::TEXT)
    {
        const char* val = lua_tostring(L, 3);
        rec_data->rec->setValueText(field, val);
    }
    else if(valtype == RecordObject::REAL)
    {
        double val = lua_tonumber(L, 3);
        rec_data->rec->setValueReal(field, val);
    }
    else if(valtype == RecordObject::INTEGER)
    {
        long val = (long)lua_tointeger(L, 3);
        rec_data->rec->setValueInteger(field, val);
    }
    else
    {
        status = false;
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_serialize - rec:serialize() --> string
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_serialize(lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    unsigned char* buffer = NULL;
    int bytes = rec_data->rec->serialize(&buffer); // memory allocated
    lua_pushlstring(L, (const char*)buffer, bytes);
    delete [] buffer; // memory freed

    return 1; // number of items returned (record string)
}

/*----------------------------------------------------------------------------
 * lmsg_deserialize - rec:deserialize(string)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deserialize(lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    size_t lbuf = 0;
    unsigned char* buffer = (unsigned char*)lua_tolstring(L, 2, &lbuf);

    bool status = rec_data->rec->deserialize(buffer, lbuf);
    lua_pushboolean(L, status);

    return 1; // number of items returned (status of deserialization)
}

/*----------------------------------------------------------------------------
 * lmsg_deleterec
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deleterec (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data)
    {
        if(rec_data->record_str) delete [] rec_data->record_str;
        if(rec_data->rec) delete rec_data->rec;
    }
    return 0;
}
