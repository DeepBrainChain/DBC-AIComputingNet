/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        container_message.h
* description    container message definition
* date                  2018.04.07
* author            Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"
#include <stringbuffer.h>
#include "container_message.h"


#define STRING_REF(VAR)                        rapidjson::StringRef(VAR.c_str(), VAR.length())


namespace matrix
{
    namespace core
    {
        enum AUTH_ERROR
        {
            AUTH_NET_ERROR = -1,
            AUTH_SUCCESS =    0
        } ;

        const int32_t DEFAULT_AUTH_REPORT_CYTLE = 60;
        class auth_task_req : public json_io_buf
        {
        public:

            std::string mining_node_id;

            std::string ai_user_node_id;

            std::string task_id;

            std::string start_time;
           
            std::string task_state;
            
            std::string end_time;

            std::string sign_type;

            std::string sign;

            std::string to_string();
        };

        class auth_task_resp : public json_io_buf
        {
        public:

            int32_t status = AUTH_NET_ERROR;

            std::string contract_state;

            int32_t report_cycle = DEFAULT_AUTH_REPORT_CYTLE;
            void from_string(const std::string & buf);
        };
    }

}