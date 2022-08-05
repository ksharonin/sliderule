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

#ifndef __atl06_proxy__
#define __atl06_proxy__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl06Proxy: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_REQUEST_PARAMETER_SIZE = 0x2000000; // 32MB
        static const int CPU_LOAD_FACTOR = 10; // number of concurrent requests per cpu
        static const int NODE_LOCK_TIMEOUT = 600; // 10 minutes

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     init        (void);
        static void     deinit      (void);
        static int      luaInit     (lua_State* L);
        static int      luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            Atl06Proxy*     proxy;
            const char*     resource;
            int             index;      // 0..number of requests
            bool            valid;      // set to false when error encountered
            bool            complete;   // set to true when request finished
            Cond            sync;       // signals when request is complete
        } atl06_rqst_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Publisher*   rqstPub;
        static Subscriber*  rqstSub;
        static bool         proxyActive;
        static Thread**     proxyPids; // thread pool
        static Mutex        proxyMut;
        static int          threadPoolSize;

        atl06_rqst_t*       requests; // array[numRequests]
        int                 numRequests;
        const char*         parameters;
        Publisher*          outQ;
        const char*         orchestratorURL;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl06Proxy              (lua_State* L, const char** _resources, int _num_resources, const char* _parameters, const char* _outq_name, const char* _orchestrator_url);
                            ~Atl06Proxy             (void);

        static void*        proxyThread             (void* parm);
};

#endif  /* __atl06_proxy__ */