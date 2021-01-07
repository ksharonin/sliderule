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

#ifndef __http_server__
#define __http_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "MsgQ.h"
#include "Table.h"
#include "OsApi.h"
#include "StringLib.h"
#include "LuaObject.h"
#include "EndpointObject.h"

/******************************************************************************
 * HTTP SERVER CLASS
 ******************************************************************************/

class HttpServer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int REQUEST_MSG_BUF_LEN        = MAX_STR_SIZE;
        static const int REQUEST_ID_LEN             = 128;
        static const int CONNECTION_TIMEOUT         = 5; // seconds
        static const int INITIAL_POLL_SIZE          = 16;
        static const int IP_ADDR_STR_SIZE           = 64;
        static const int DEFAULT_MAX_CONNECTIONS    = 256;
        static const int STREAM_OVERHEAD_SIZE       = 128; // chunk size, record size, and line breaks

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);

                            HttpServer      (lua_State* L, const char* _ip_addr, int _port, int max_connections);
                            ~HttpServer     (void);

        const char*         getUniqueId     (void);
        const char*         getIpAddr       (void);
        int                 getPort         (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int                         header_index;
            bool                        header_complete;
            bool                        header_sent;
            bool                        response_complete;
            Subscriber::msgRef_t        ref;
            int                         ref_status;
            int                         ref_index;
            Subscriber*                 rspq;
            uint8_t*                    stream_buf;
            int                         stream_buf_index;
            int                         stream_buf_size;
            int                         stream_mem_size;
        } state_t;

        typedef struct {
            SafeString                  message; // raw request
            EndpointObject::request_t   request;
            state_t                     state;
        } connection_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint64_t>    requestId;

        bool                            active;
        Thread*                         listenerPid;
        Table<connection_t*, int>       connections;

        Dictionary<EndpointObject*>     routeTable;

        char*                           ipAddr;
        int                             port;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*        listenerThread      (void* parm);

        static void         extract             (const char* url, char** endpoint, char** new_url);

        static int          luaAttach           (lua_State* L);

        static int          pollHandler         (int fd, short* events, void* parm);
        static int          activeHandler       (int fd, int flags, void* parm);
        int                 onRead              (int fd);
        int                 onWrite             (int fd);
        int                 onAlive             (int fd);
        int                 onConnect           (int fd);
        int                 onDisconnect        (int fd);
};

#endif  /* __http_server__ */
