/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name            :    api_call.h
* description        : 
* date                      :   2018/5/8
* author                 :    Jimmy Kuang
**********************************************************************************/
#pragma once

#include "matrix_types.h"


namespace matrix
{
    namespace service_core
    {
        enum get_peers_flag
        {
            flag_active,
            flag_global
        };

        class cmd_network_address {
        public:

            std::string ip;

            uint16_t port;
        };

        class cmd_peer_node_info {
        public:

            std::string peer_node_id;

            int32_t live_time_stamp;

            int8_t net_st = -1;

            cmd_network_address addr;

            int8_t node_type = 0;

            std::vector<std::string> service_list;

            cmd_peer_node_info &operator=(const matrix::service_core::peer_node_info &info) {
                peer_node_id = info.peer_node_id;
                live_time_stamp = info.live_time_stamp;
                net_st = -1;
                addr.ip = info.addr.ip;
                addr.port = (uint16_t)info.addr.port;
                //node_type = info.
                service_list = info.service_list;
                return *this;
            }
        };

        class cmd_get_peer_nodes_req : public matrix::core::msg_base {
        public:

            get_peers_flag flag;

        };

        class cmd_get_peer_nodes_resp : public matrix::core::msg_base {
        public:
            int32_t result;

            std::string result_info;

            std::vector<cmd_peer_node_info> peer_nodes_list;
        };
    }//namespace service_core
}//namespace matrix
