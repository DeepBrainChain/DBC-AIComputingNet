/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   peer_node.cpp
* description    :   p2p network peer node
* date                  :   2018.03.29
* author            :   Bruce Feng
**********************************************************************************/


#include "peer_node.h"


namespace matrix
{
    namespace service_core
    {

        peer_node::peer_node()
            : m_core_version(0)
            , m_protocol_version(0)
            , m_connected_time(0)
            , m_live_time(0)
            , m_connection_status(DISCONNECTED)
            , m_node_type(NORMAL_NODE)
        {

        }

        peer_node::peer_node(const peer_node &src)
        {
            *this = src;
        }

        peer_node& peer_node::operator=(const peer_node &src)
        {
            if (this == &src)
            {
                return *this;
            }

            this->m_id = src.m_id;
            this->m_sid = src.m_sid;
            this->m_core_version = src.m_core_version;
            this->m_protocol_version = src.m_protocol_version;
            this->m_connected_time = src.m_connected_time;
            this->m_live_time = src.m_live_time;
            this->m_connection_status = src.m_connection_status;
            this->m_peer_addr = src.m_peer_addr;
            this->m_local_addr = src.m_local_addr;
            this->m_node_type = src.m_node_type;

            return *this;
        }

        void assign_peer_info(peer_node_info& info, const std::shared_ptr<peer_node> node)
        {
            info.peer_node_id = node->m_id;
            info.live_time_stamp = (int32_t)node->m_live_time;
            info.addr.ip = node->m_peer_addr.get_ip();
            info.addr.port = node->m_peer_addr.get_port();
        }
    }

}