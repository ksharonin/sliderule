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

#ifndef __lua_endpoint__
#define __lua_endpoint__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "EndpointObject.h"
#include "OsApi.h"
#include "StringLib.h"
#include "Dictionary.h"
#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"

/******************************************************************************
 * PISTACHE SERVER CLASS
 ******************************************************************************/

class LuaEndpoint: public EndpointObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const char* EndpointExceptionRecType;
        static const RecordObject::fieldDef_t EndpointExceptionRecDef[];

        static const double DEFAULT_NORMAL_REQUEST_MEMORY_THRESHOLD;
        static const double DEFAULT_STREAM_REQUEST_MEMORY_THRESHOLD;

        static const int MAX_SOURCED_RESPONSE_SIZE = 0x8000000; // 128M
        static const int MAX_RESPONSE_TIME_MS = 5000;
        static const int MAX_EXCEPTION_TEXT_SIZE = 256;
        static const char* LUA_RESPONSE_QUEUE;
        static const char* LUA_REQUEST_ID;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Response Exception Record */
        typedef struct {
            int32_t code;
            int32_t level;
            char    text[MAX_EXCEPTION_TEXT_SIZE];
        } response_exception_t;

        /*--------------------------------------------------------------------
         * Authenticator Subclass
         *--------------------------------------------------------------------*/

        class Authenticator: public LuaObject
        {
            public:
                static const char* OBJECT_TYPE;
                static const char* LUA_META_NAME;
                static const struct luaL_Reg LUA_META_TABLE[];

                Authenticator(lua_State* L);
                virtual ~Authenticator(void);

                virtual bool isValid(const char* token) = 0;
       };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static SafeString serverHead;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init                    (void);
        static int          luaCreate               (lua_State* L);
        static void         generateExceptionStatus (int code, event_level_t level, Publisher* outq, bool* active, const char* errmsg, ...) VARG_CHECK(printf, 5, 6);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaEndpoint     (lua_State* L, double normal_mem_thresh, double stream_mem_thresh, event_level_t lvl);
        virtual             ~LuaEndpoint    (void);

        static void*        requestThread   (void* parm);

        rsptype_t           handleRequest   (Request* request) override;

        void                normalResponse  (const char* scriptpath, Request* request, Publisher* rspq, uint32_t trace_id) const;
        void                streamResponse  (const char* scriptpath, Request* request, Publisher* rspq, uint32_t trace_id) const;

        static int          luaAuth         (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        double              normalRequestMemoryThreshold;
        double              streamRequestMemoryThreshold;
        event_level_t       logLevel;
        Authenticator*      authenticator;
};

#endif  /* __lua_endpoint__ */
