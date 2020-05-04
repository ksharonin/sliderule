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

#include "CcsdsRecordDispatcher.h"
#include "CcsdsRecord.h"
#include "core.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - dispatcher(<input stream name>, [<num threads>], [<key mode>, <key parm>])
 *----------------------------------------------------------------------------*/
int CcsdsRecordDispatcher::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* qname           = getLuaString(L, 1);
        long        num_threads     = getLuaInteger(L, 2, true, LocalLib::nproc());
        const char* key_mode_str    = getLuaString(L, 3, true, "RECEIPT_KEY");

        /* Check Number of Threads */
        if(num_threads < 1)
        {
            throw LuaException("invalid number of threads supplied (must be >= 1)");
        }

        /* Set Key Mode */
        keyMode_t key_mode = str2mode(key_mode_str);
        const char* key_field = NULL;
        calcFunc_t  key_func = NULL;
        if(key_mode == INVALID_KEY_MODE)
        {
            throw LuaException("Invalid key mode specified: %s\n", key_mode_str);
        }
        else if(key_mode == FIELD_KEY_MODE)
        {
            key_field = getLuaString(L, 4);
        }
        else if(key_mode == CALCULATED_KEY_MODE)
        {
            const char* key_func_str = getLuaString(L, 4);
            key_func = keyCalcFunctions[key_func_str];
        }

        /* Create Record Dispatcher */
        return createLuaObject(L, new CcsdsRecordDispatcher(L, qname, key_mode, key_field, key_func, num_threads));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
    catch(std::out_of_range& e)
    {
        (void)e;
        mlog(CRITICAL, "Invalid calculation function provided - no handler installed\n");
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsRecordDispatcher::CcsdsRecordDispatcher(lua_State* L, const char* inputq_name, keyMode_t key_mode, const char* key_field, calcFunc_t key_func, int num_threads):
    RecordDispatcher(L, inputq_name, key_mode, key_field, key_func, num_threads)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsRecordDispatcher::~CcsdsRecordDispatcher(void)
{
}

/*----------------------------------------------------------------------------
 * createRecord
 *----------------------------------------------------------------------------*/
RecordObject* CcsdsRecordDispatcher::createRecord (unsigned char* buffer, int size)
{
    return new CcsdsRecordInterface(buffer, size);
}
