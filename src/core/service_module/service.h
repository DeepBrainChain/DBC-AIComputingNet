/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：service.h
* description    ：service
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _SERVICE_H_
#define _SERVICE_H_


typedef void * SERVICE_HANDLE;


//service invoke template function
int32_t int32_t invoke_xxx(SERVICE_HANDLE service_handle, MESSAGE_PTR message);


class Service
{
public:

    virtual int32_t invoke(MESSAGE_PTR message ) = 0;

};

#endif


