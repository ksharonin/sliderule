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

#ifndef __h5_array__
#define __h5_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "StringLib.h"
#include "LogLib.h"
#include "H5Lib.h"

/******************************************************************************
 * H5Array TEMPLATE
 ******************************************************************************/

template <class T>
class H5Array
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                H5Array     (const char* url, const char* dataset, unsigned col=0, unsigned startrow=0, unsigned numrows=0);
        virtual ~H5Array    (void);

        T&      operator[]  (int index);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*     name;
        int32_t         size;
        T*              data;
};

/******************************************************************************
 * H5Array METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::H5Array(const char* url, const char* dataset, unsigned col, unsigned startrow, unsigned numrows)
{
    name = NULL;
    data = NULL;
    size = 0;
    
    H5Lib::info_t info = H5Lib::read(url, dataset, RecordObject::DYNAMIC, col, startrow, numrows);

    name = StringLib::duplicate(dataset);
    data = (T*)info.data;
    size = info.elements;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::~H5Array(void)
{
    if(name) delete [] name;
    if(data) delete [] data;
}

/*----------------------------------------------------------------------------
 * []]
 *----------------------------------------------------------------------------*/
template <class T>
T& H5Array<T>::operator[](int index)
{
    return data[index];
}

#endif  /* __h5_array__ */
