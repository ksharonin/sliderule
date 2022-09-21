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

#include "S3Client.h"
#include "CredentialStore.h"
#include "core.h"

#include <curl/curl.h>
#include <openssl/hmac.h>

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

class S3Client::impl
{
    public:

        static const long DEFAULT_CONNECTION_TIMEOUT = 10; // seconds

        S3Client::impl(CredentialStore::Credential* _credential, const char* _endpoint, const char* _region);
        S3Client::~impl(void);

        void get (const char* bucket, const char* key, long offset, long length, uint8_t* buffer, int timeout_secs);

    private:

        typedef struct {
            uint8_t*    buffer;
            int         size;
            int         index;
        } data_t;

        CredentialStore::Credential* credential;
        const char* endpoint;
        const char* region;
        CURL* curl;
};

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
S3Client::impl (CredentialStore::Credential* _credential, const char* _endpoint, const char* _region):
    credential(_credential),
    endpoint(_endpoint),
    region(_region),
    bufferIndex(0)
{
    /* Initialize cURL */
    curl = curl_easy_init();
    if(curl)
    {
        /* Set cURL Options */
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECTION_TIMEOUT); // seconds
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, S3Client::curlWriteCallback);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to initialize curl");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3Client::~impl (void)
{
    curl_easy_cleanup(curl);
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
void S3Client::impl::get (const char* bucket, const char* key, long offset, long length, uint8_t* buffer, int timeout_secs)
{
    /* Build Host and URL String */
    SafeString host("%s.%s.s3.amazonaws.com", bucket, region);
    SafeString url("https://%s/%s", host.getString(), key);

    /* Build Date String */
    TimeLib::gmt_time_t gmt_time = TimeLib::gettime();
    TimeLib::date_t gmt_date = TimeLib::gmt2date(gmt_time);
    SafeString date("%04d%02d%02dT%02d%02d%02dZ", gmt_date.year, gmt_date.month, gmt_date.day, gmt_time.hour, gmt_time.minute, gmt_time.second);

    /* Build SecurityToken Header */
    SafeString securityTokenHeader("x-amz-security-token:%s", credentials->sessionToken);

    /* Build StringToSign */
    SafeString stringToSign("GET\n\n\n%s\n%s\n/%s/%s", date.getString(), securityTokenHeader.getString(), bucket, key);

    /* Build Authorization Header */
    char hash[EVP_MAX_MD_SIZE];
    int hash_size = EVP_MAX_MD_SIZE;
    HMAC(EVP_sha1(), credential->secretAccessKey, strlen(credential->secretAccessKey), stringToSign.getString(), stringToSign.getLength() - 1, hash, &hash_size);
    SafeString authorizationHeader("Authorization: AWS %s:%s", credential->accessKeyId, stringToSign.getString());


    /* Setup Buffer for Callback */
    data_t data = {
        .buffer = buffer;
        .size = length,
        .index = 0
    };
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    /* Set Options */
    curl_easy_setopt(curl, CURLOPT_URL, url.getString());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_secs);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);

    /* Build Request Headers */
    // TODO

    /* Perform Request */
    CURLcode res = curl_easy_perform(curl);
    if(res == CURLE_OK)
    {
        /* Get HTTP Code */
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if(http_code != 200)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Http error <%ld> returned from S3 request", http_code);
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "cURL request to S3 failed: %ld", (long)res);
    }
}

/*----------------------------------------------------------------------------
 * writeData
 *----------------------------------------------------------------------------*/
size_t S3Client::curlWriteCallback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    data_t* data = (data_t*)userp;

    size_t rsps_size = size * nmemb;
    size_t bytes_available = data->size - data->index;
    size_t bytes_to_copy = MIN(rsps_size, bytes_available);

    LocalLib::copy(&data->buffer[data->index], buffer, bytes_to_copy);
    data->index += bytes_to_copy;

    return bytes_to_copy;
}

/******************************************************************************
 * AWS S3 LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/
Mutex S3Client::clientsMut;
Dictionary<S3Client*> S3Client::clients(STARTING_NUM_CLIENTS);

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void S3Client::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void S3Client::deinit (void)
{
    clientsMut.lock();
    {
        S3Client* client;
        const char* key = clients.first(&client);
        while(key != NULL)
        {
            delete client->s3_client;
            delete [] client->asset_name;
            delete client;
            key = clients.next(&client);
        }
    }
    clientsMut.unlock();
}

/*----------------------------------------------------------------------------
 * constructor
 *----------------------------------------------------------------------------*/
S3Client::S3Client (const Asset* asset)
{
    client = NULL;

    /* Get Latest Credentials */
    CredentialStore::Credential latest_credential = CredentialStore::get(asset->getName());

    clientsMut.lock();
    {
        /* Try to Get Existing Client */
        if(clients.find(asset->getName(), &client))
        {
            client->reference_count++;
        }

        /* Check Need for New Client */
        if( (client == NULL) || // could not find an existing client
            (latest_credential.provided && (client->credential.expirationGps < latest_credential.expirationGps)) ) // existing client has outdated credentials
        {
            /* Destroy Old Client */
            if(client)
            {
                client->decommissioned = true;
                destroyClient(client);
            }

            /* Create Client */
            client = new client_t;
            client->credential = latest_credential;
            client->asset_name = StringLib::duplicate(asset->getName());
            client->reference_count = 1;
            client->decommissioned = false;
            client->s3_handle = new S3Client::impl(&client->credential, asset->getEndpoint(), asset->getRegion());

            /* Register New Client */
            clients.add(asset->getName(), client);
        }
    }
    clientsMut.unlock();

    /* Return Client */
    return client;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
S3Client::~S3Client (void)
{
    destroyClient();
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int S3Client::readBuffer (void* buf, int len, int timeout)
{
    int rc = len;

    try
    {
        client->s3_handle->get(buf, len, (timeout + 999) / 1000);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "S3 client failure: %s", e.what());
        rc = INVALID_RC;
    }

    return rc;
}

/*----------------------------------------------------------------------------
 * destroyClient
 *----------------------------------------------------------------------------*/
void S3Client::destroyClient (void)
{
    assert(client);
    assert(client->reference_count > 0);

    clientsMut.lock();
    {
        client->reference_count--;
        if(client->decommissioned && (client->reference_count == 0))
        {
            clients.remove(client->asset_name);
            delete client->s3_client;
            delete [] client->asset_name;
            delete client->s3_handle;
            delete client;
        }
    }
    clientsMut.unlock();
}