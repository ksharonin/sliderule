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

#include <atomic>

#include "HttpServer.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* HttpServer::OBJECT_TYPE = "HttpServer";
const char* HttpServer::LuaMetaName = "HttpServer";
const struct luaL_Reg HttpServer::LuaMetaTable[] = {
    {"attach",      luaAttach},
    {NULL,          NULL}
};

std::atomic<uint64_t> HttpServer::requestId{0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - server(<ip_addr>, <port>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         port      = (int)getLuaInteger(L, 1);
        const char* ip_addr   = getLuaString(L, 2, true, NULL);

        /* Get Server Parameter */
        if( ip_addr && (StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*")) )
        {
            ip_addr = NULL;
        }

        /* Return File Device Object */
        return createLuaObject(L, new HttpServer(L, ip_addr, port));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating HttpServer: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpServer::HttpServer(lua_State* L, const char* _ip_addr, int _port):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;

    active = true;
    listenerPid = new Thread(listenerThread, this);

    dataToWrite = false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
HttpServer::~HttpServer(void)
{
    active = false;
    delete listenerPid;

    if(ipAddr) delete [] ipAddr;

    EndpointObject* endpoint;
    const char* key = routeTable.first(&endpoint);
    while(key != NULL)
    {
        endpoint->releaseLuaObject();
        key = routeTable.next(&endpoint);
    }
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
const char* HttpServer::getUniqueId (void)
{
    char* id_str = new char [MAX_STR_SIZE];
    long id = requestId++;
    StringLib::format(id_str, REQUEST_ID_LEN, "%s:%d:%ld", getIpAddr(), getPort(), id);
    return id_str;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* HttpServer::getIpAddr (void)
{
    if(ipAddr)  return ipAddr;
    else        return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int HttpServer::getPort (void)
{
    return port;
}

/*----------------------------------------------------------------------------
 * listenerThread
 *----------------------------------------------------------------------------*/
void* HttpServer::listenerThread(void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    int status = SockLib::startserver (s->getIpAddr(), s->getPort(), IO_INFINITE_CONNECTIONS, pollHandler, activeHandler, (void*)s);
    if(status < 0) mlog(CRITICAL, "Failed to establish http server on %s:%d (%d)\n", s->getIpAddr(), s->getPort(), status);

    return NULL;
}

/*----------------------------------------------------------------------------
 * extract
 * 
 *  Note: must delete returned strings
 *----------------------------------------------------------------------------*/
void HttpServer::extract (const char* url, char** endpoint, char** new_url)
{
    const char* src;
    char* dst;

    *endpoint = NULL;
    *new_url = NULL;

    const char* first_slash = StringLib::find(url, '/');
    if(first_slash)
    {
        const char* second_slash = StringLib::find(first_slash+1, '/');
        if(second_slash)
        {
            /* Get Endpoint */
            int endpoint_len = second_slash - first_slash; // this includes null terminator
            *endpoint = new char[endpoint_len];
            src = first_slash ; // include the slash
            dst = *endpoint;
            while(src < second_slash) *dst++ = *src++;
            *dst = '\0';

            /* Get New URL */
            const char* terminator = StringLib::find(second_slash+1, '\0');
            if(terminator)
            {
                int new_url_len = terminator - second_slash; // this includes null terminator
                *new_url = new char[new_url_len];
                src = second_slash + 1; // do NOT include the slash
                dst = *new_url;
                while(src < terminator) *dst++ = *src++;
                *dst = '\0';
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * luaAttach - :attach(<EndpointObject>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaAttach (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        HttpServer* lua_obj = (HttpServer*)getLuaSelf(L, 1);

        /* Get Parameters */
        EndpointObject* endpoint = (EndpointObject*)getLuaObject(L, 2, EndpointObject::OBJECT_TYPE);
        const char* url = getLuaString(L, 3);

        /* Add Route to Table */
        status = lua_obj->routeTable.add(url, endpoint, true);

        /* Check Need to Unlock Unregistered Endpoint */
        if(status != true)
        {
            endpoint->releaseLuaObject();
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error attaching handler: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * pollHandler
 *
 *  Notes: provides the flags back to the poll function
 *----------------------------------------------------------------------------*/
int HttpServer::pollHandler(int* flags, void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    /* Check Alive */
    if(!s->active) return -1;

    /* Set Polling Flags */
    int pollflags = IO_READ_FLAG;
    if(s->dataToWrite) pollflags |= IO_WRITE_FLAG;
    s->dataToWrite = false;

    /* Return Polling Flags */
    *flags = pollflags;

    return 0;
}

/*----------------------------------------------------------------------------
 * activeHandler
 *
 *  Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int HttpServer::activeHandler(int fd, int flags, void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    int rc = 0;

    if((flags & IO_ALIVE_FLAG)      && (s->onAlive(fd)      < 0))   rc = INVALID_RC;
    if((flags & IO_READ_FLAG)       && (s->onRead(fd)       < 0))   rc = INVALID_RC;
    if((flags & IO_WRITE_FLAG)      && (s->onWrite(fd)      < 0))   rc = INVALID_RC;
    if((flags & IO_CONNECT_FLAG)    && (s->onConnect(fd)    < 0))   rc = INVALID_RC;
    if((flags & IO_DISCONNECT_FLAG) && (s->onDisconnect(fd) < 0))   rc = INVALID_RC;

    return rc;
}

/*----------------------------------------------------------------------------
 * onRead
 *
 *  Notes: performed for every connection that is ready to have data read from it
 *----------------------------------------------------------------------------*/
int HttpServer::onRead(int fd)
{
    int status = 0;
    char msg_buf[REQUEST_MSG_BUF_LEN];
    connection_t* connection = connections[fd];

    int bytes = SockLib::sockrecv(fd, msg_buf, REQUEST_MSG_BUF_LEN - 1, IO_CHECK);
    if(bytes > 0)
    {
        msg_buf[bytes] = '\0';
        connection->message += msg_buf;

        /* Look Through Existing Header Received */
        while(!connection->state.header_complete && (connection->state.header_index < (connection->message.getLength() - 4)))
        {
            /* If Header Complete (look for \r\n\r\n separator) */ 
            if( (connection->message[connection->state.header_index + 0] == '\r') && 
                (connection->message[connection->state.header_index + 1] == '\n') &&
                (connection->message[connection->state.header_index + 2] == '\r') &&
                (connection->message[connection->state.header_index + 3] == '\n') )
            {
                /* Parse Request */    
                connection->message.setChar('\0', connection->state.header_index);
                List<SafeString>* header_list = connection->message.split('\r');
                connection->message.setChar('\r', connection->state.header_index);
                connection->state.header_complete = true;
                connection->state.header_index += 4; // moves state.header_index to start of body

                /* Parse Request Line */
                try
                {
                    List<SafeString>* request_line = (*header_list)[0].split(' ');
                    const char* verb_str = (*request_line)[0].getString();
                    const char* url_str = (*request_line)[1].getString();

                    /* Get Verb */
                    connection->request.verb = EndpointObject::str2verb(verb_str);

                    /* Get Endpoint and URL */
                    char* endpoint = NULL;
                    extract(url_str, &endpoint, &connection->request.url);
                    if(endpoint && connection->request.url)
                    {
                        try
                        {
                            /* Get Attached Endpoint Object */
                            connection->request.endpoint = routeTable[endpoint];
                        }
                        catch(const std::out_of_range& e)
                        {
                            mlog(CRITICAL, "No attached endpoint at %s: %s\n", endpoint, e.what());
                            status = INVALID_RC; // will close socket
                        }                    
                    }
                    else
                    {
                        mlog(CRITICAL, "Enable to extract endpoint and url: %s\n", url_str);
                    }

                    /* Clean Up Allocated Memory */
                    delete request_line;
                    if(endpoint) delete [] endpoint;
                }
                catch(const std::out_of_range& e)
                {
                    mlog(CRITICAL, "Invalid request line: %s: %s\n", (*header_list)[0].getString(), e.what());
                }

                /* Parse Headers */
                for(int h = 1; h < header_list->length(); h++)
                {
                    /* Create Key/Value Pairs */
                    List<SafeString>* keyvalue_list = (*header_list)[h].split(':');
                    try
                    {
                        const char* key = (*keyvalue_list)[0].getString();
                        const char* value = (*keyvalue_list)[1].getString(true);
                        connection->request.headers->add(key, value, true);
                    }
                    catch(const std::out_of_range& e)
                    {
                        mlog(CRITICAL, "Invalid header in http request: %s: %s\n", (*header_list)[h].getString(), e.what());
                    }
                    delete keyvalue_list;                
                }

                /* Clean Up Header List */
                delete header_list;

                /* Get Content Length */
                try
                {
                    if(!StringLib::str2long(connection->request.headers->get("Content-Length"), &connection->request.body_length))
                    {
                        mlog(CRITICAL, "Invalid Content-Length header: %s\n", connection->request.headers->get("Content-Length"));
                        status = INVALID_RC; // will close socket
                    }
                }
                catch(const std::out_of_range& e)
                {
                    mlog(CRITICAL, "Http request must supply Content-Length header: %s\n", e.what());
                    status = INVALID_RC; // will close socket
                }
            }
            else
            {
                /* Go to Next Character in Header */
                connection->state.header_index++;
            }
        }

        /* Check If Body Complete */
        if(connection->request.body_length > 0)
        {
            if((connection->message.getLength() - connection->state.header_index) >= connection->request.body_length)
            {
                /* Get Message Body */
                const char* raw_message = connection->message.getString();
                connection->request.body = &raw_message[connection->state.header_index];

                /* Handle Request */
                if(connection->request.endpoint)
                {
                    connection->request.endpoint->handleRequest(&connection->request);
                }
                else
                {
                    mlog(CRITICAL, "Unable to handle unattached request\n");
                    status = INVALID_RC; // will close socket
                }
            }
        }
    }
    else
    {
        /* Failed to receive data on socket that was marked for reading */
        status = INVALID_RC; // will close socket
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onWrite
 *
 *  Notes: performed for every request that is ready to have data written to it
 *----------------------------------------------------------------------------*/
int HttpServer::onWrite(int fd)
{
    int status = 0;
    connection_t* connection = connections[fd];

    /* Send Data */
    if((connection->state.ref_status > 0) && 
       (connection->state.ref_index < connection->state.ref.size))
    {
        int bytes_left = connection->state.ref.size - connection->state.ref_index;
        uint8_t* buffer = (uint8_t*)connection->state.ref.data;
        int bytes = SockLib::socksend(fd, &buffer[connection->state.ref_index], bytes_left, IO_CHECK);
        if(bytes > 0)
        {
            connection->state.ref_index += bytes;
        }
        else
        {
            /* Failed to send data on socket that was marked for writing */
            status = INVALID_RC; // will close socket
        }
    }

    /* Check if Done with Current Reference */
    if((connection->state.ref_status > 0) && 
       (connection->state.ref_index == connection->state.ref.size))
    {
        connection->state.rspq->dereference(connection->state.ref);

        /* Check if Done with Entire Response 
         *  a valid reference of size zero indicates that
         *  the response is complete */
        if(connection->state.ref.size == 0)
        {
            status = INVALID_RC; // will close socket
        }

        /* Reset Reference State */
        connection->state.ref_status = 0;
        connection->state.ref_index = 0;
        connection->state.ref.size = 0;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onAlive
 *
 *  Notes: Performed for every existing connection
 *----------------------------------------------------------------------------*/
int HttpServer::onAlive(int fd)
{
    connection_t* connection = connections[fd];

    if(connection->state.ref_status <= 0)
    {
        connection->state.ref_status = connection->state.rspq->receiveRef(connection->state.ref, IO_CHECK);
        if(connection->state.ref_status > 0)
        {
            /* Mark Ready to Write */
            dataToWrite = true;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onConnect
 *
 *  Notes: performed on new connections when the connection is made
 *----------------------------------------------------------------------------*/
int HttpServer::onConnect(int fd)
{
    int status = 0;

    /* Create and Initialize New Request */
    connection_t* connection = new connection_t;
    LocalLib::set(&connection->request, 0, sizeof(EndpointObject::request_t));
    LocalLib::set(&connection->state, 0, sizeof(state_t));

    /* Register Connection */
    if(connections.add(fd, connection, true))
    {
        connection->request.id = getUniqueId(); // id is allocated
        connection->request.headers = new Dictionary<const char*>();
        connection->state.rspq = new Subscriber(connection->request.id);
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to register connection due to duplicate entry\n", connection->request.id);
        status = INVALID_RC;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onDisconnect
 *
 *  Notes: performed on disconnected connections
 *----------------------------------------------------------------------------*/
int HttpServer::onDisconnect(int fd)
{
    int status = 0;

    connection_t* connection = connections[fd];
    if(connections.remove(fd))
    {
        /* Clear Out Headers */
        const char* header;
        const char* key = connection->request.headers->first(&header);
        while(key != NULL)
        {
            /* Free Header Value */
            delete [] header;
            key = connection->request.headers->next(&header);
        }

        /* Free URL */
        if(connection->request.url) delete [] connection->request.url;

        /* Free Rest of Allocated Connection Members */
        delete [] connection->request.id; 
        delete connection->request.headers;
        delete connection->state.rspq;

        /* Free Connection */
        delete connection;
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to release connection\n", connection->request.id);
        status = -1;
    }

    return status;
}

