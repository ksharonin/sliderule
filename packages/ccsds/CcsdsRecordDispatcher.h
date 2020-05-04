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

#ifndef __ccsds_record_dispatcher__
#define __ccsds_record_dispatcher__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RecordDispatcher.h"

/******************************************************************************
 * CCSDS RECORD DISPATCHER CLASS
 ******************************************************************************/

class CcsdsRecordDispatcher: public RecordDispatcher
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsRecordDispatcher   (lua_State* L, const char* inputq_name, keyMode_t key_mode, const char* key_field, calcFunc_t key_func, int num_threads);
                        ~CcsdsRecordDispatcher  (void);

        RecordObject*   createRecord            (unsigned char* buffer, int size);
};

#endif  /* __ccsds_record_dispatcher__ */
