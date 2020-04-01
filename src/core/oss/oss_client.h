/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_client.h
* description    :   container client for definition
* date                  :   2018.07.020
* author            :   Regulus
**********************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "http_client.h"
#include "bill/bill_message.h"
#include "idle_task/idle_task_message.h"



using namespace std;

namespace matrix
{
    namespace core
    { 
        class oss_client
        {
        public:
            oss_client(const std::string & url, const std::string &crt);

            //auth_task
            std::shared_ptr<auth_task_resp> post_auth_task(std::shared_ptr<auth_task_req> req);
            int32_t  post_auth_stop_task(std::shared_ptr<auth_task_req> req);

            //idle_task
            std::shared_ptr<idle_task_resp> post_idle_task(std::shared_ptr<idle_task_req> req);

            ~oss_client() = default;
        protected:

            http_client m_http_client;

            std::string m_url;
            std::string m_crt;
        };

    }

}
