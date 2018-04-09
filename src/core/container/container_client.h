/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºcontainer_client.h
* description    £ºcontainer client for definition
* date                  : 2018.04.04
* author            £ºBruce Feng
**********************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "http_client.h"
#include "container_message.h"


using namespace std;


#define DEFAULT_STOP_CONTAINER_TIME                         10


namespace matrix
{
    namespace core
    { 

        class container_client
        {
        public:

            container_client(std::string remote_ip, uint16_t remote_port);

            ~container_client() = default;

            //create
            std::shared_ptr<container_create_resp> create_container(std::shared_ptr<container_config> config);

            std::shared_ptr<container_create_resp> create_container(std::shared_ptr<container_config> config, std::string name);

            //start
            int32_t start_container(std::string container_id);

            int32_t start_container(std::string container_id, std::shared_ptr<container_host_config> config);

            //stop
            int32_t stop_container(std::string container_id);

            int32_t stop_container(std::string container_id, int32_t timeout);

            //wait
            int32_t wait_container(std::string container_id);

            //remove
            int32_t remove_container(std::string container_id);

            int32_t remove_container(std::string container_id, bool remove_volumes);

            //inspect
            std::shared_ptr<container_inspect_response> inspect_container(std::string container_id);


        protected:

            http_client m_http_client;

            std::string m_remote_ip;

            uint16_t m_remote_port;

        };

    }

}
