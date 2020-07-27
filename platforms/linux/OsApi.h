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

#ifndef __osapi__
#define __osapi__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/******************************************************************************
 * MACROS
 ******************************************************************************/

#ifndef NULL
#define NULL        ((void *) 0)
#endif

#ifndef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

#ifndef CompileTimeAssert
#define CompileTimeAssert(Condition, Message) typedef char Message[(Condition) ? 1 : -1]
#endif

#ifdef _GNU_
#define VARG_CHECK(f, a, b) __attribute__((format(f, a, b)))
#else
#define VARG_CHECK(f, a, b)
#endif

#define PATH_DELIMETER '/'
#define PATH_DELIMETER_STR "/"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* File */
typedef FILE* fileptr_t;

/* Check Sizes */
CompileTimeAssert(sizeof(bool)==1, TypeboolWrongSize);

/******************************************************************************
 * DEFINES
 ******************************************************************************/

/* Return Codes */
#define TIMEOUT_RC                  (0)
#define INVALID_RC                  (-1)
#define SHUTDOWN_RC                 (-2)
#define TCP_ERR_RC                  (-3)
#define UDP_ERR_RC                  (-4)
#define SOCK_ERR_RC                 (-5)
#define BUFF_ERR_RC                 (-6)
#define WOULDBLOCK_RC               (-7)
#define PARM_ERR_RC                 (-8)
#define TTY_ERR_RC                  (-9)
#define ACC_ERR_RC                  (-10)

/* I/O Definitions */
#define IO_PEND                     (-1)
#define IO_CHECK                    (0)
#define IO_DEFAULT_TIMEOUT          (1000) // ms
#define IO_DEFAULT_MAXSIZE          (0x10000) // bytes
#define IO_INFINITE_CONNECTIONS     (-1)
#define IO_ALIVE_FLAG               (0x01)
#define IO_READ_FLAG                (0x02)
#define IO_WRITE_FLAG               (0x04)
#define IO_CONNECT_FLAG             (0x08)
#define IO_DISCONNECT_FLAG          (0x10)
#define SYS_TIMEOUT                 (LocalLib::getIOTimeout()) // ms
#define SYS_MAXSIZE                 (LocalLib::getIOMaxsize()) // bytes

/* Strings */
#define MAX_STR_SIZE                1024

/* Debug Logging */
#define dlog(...)                   LocalLib::print(__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Thread.h"
#include "Mutex.h"
#include "Cond.h"
#include "Sem.h"
#include "Timer.h"
#include "LocalLib.h"
#include "SockLib.h"
#include "TTYLib.h"

#endif  /* __osapi__ */
