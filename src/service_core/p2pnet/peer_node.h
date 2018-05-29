/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   peer_node.h
* description    :   p2p network peer node
* date                  :   2018.03.29
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include <string>
#include "endpoint_address.h"
#include "socket_id.h"
#include "matrix_types.h"


using namespace std;
using namespace matrix::core;


namespace matrix
{
    namespace service_core
    {

        enum connection_status
        {
            disconnected = 0,

            connected
        };

        class peer_node
        {
			friend class p2p_net_service;

            friend void assign_peer_info(peer_node_info& info, const std::shared_ptr<peer_node> node);

        public:

            peer_node() = default;

            virtual ~peer_node() = default;

            peer_node(const peer_node &src);

            peer_node& operator=(const peer_node &src);

        protected:

            std::string m_id;                               //peer node_id

            socket_id m_sid;                                //if connected directly, it has socket id

            int32_t m_core_version;

            int32_t m_protocol_version;

            int64_t m_connected_time;

            int64_t m_live_time;                            //active time, it doesn't work right now

            connection_status m_connection_status;

            endpoint_address m_peer_addr;

            endpoint_address m_local_addr;                   //local addr

        };

        extern void assign_peer_info(peer_node_info& info, const std::shared_ptr<peer_node> node);

    }

};
