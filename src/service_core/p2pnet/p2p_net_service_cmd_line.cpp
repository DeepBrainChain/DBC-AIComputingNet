/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name           :    p2p_net_service_cmd_line.cpp
* description         :    callback to cmd from cli or restful api
* date                :
* author              :
**********************************************************************************/
#include "p2p_net_service.h"
#include <cassert>
#include "server.h"
#include "conf_manager.h"
#include "tcp_acceptor.h"
#include "service_message_id.h"
#include "service_message_def.h"
#include "matrix_types.h"
#include "matrix_client_socket_channel_handler.h"
#include "matrix_server_socket_channel_handler.h"

#include "channel.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "api_call.h"
#include "id_generator.h"
#include "common/version.h"
#include "tcp_socket_channel.h"
#include "timer_def.h"
#include "peer_seeds.h"
#include "peers_db_types.h"
#include <leveldb/write_batch.h>
#include <boost/format.hpp>

#include "ai_crypter.h"

using namespace std;
using namespace matrix::core;
using namespace matrix::service_core;


namespace matrix
{
    namespace service_core
    {
        int32_t p2p_net_service::on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg)
        {
            auto cmd_resp = std::make_shared<matrix::service_core::cmd_get_peer_nodes_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            std::shared_ptr<base> content = msg->get_content();
            auto req = std::dynamic_pointer_cast<matrix::service_core::cmd_get_peer_nodes_req>(content);
            assert(nullptr != req && nullptr != content);
            COPY_MSG_HEADER(req,cmd_resp);
            if (!req || !content)
            {
                LOG_ERROR << "null ptr of cmd_get_peer_nodes_req";
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error";
                TOPIC_MANAGER->publish<void>(typeid(matrix::service_core::cmd_get_peer_nodes_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            if(req->flag == matrix::service_core::flag_active)
            {
                for (auto itn = m_peer_nodes_map.begin(); itn != m_peer_nodes_map.end(); ++itn)
                {
                    matrix::service_core::cmd_peer_node_info node_info;
                    node_info.peer_node_id = itn->second->m_id;
                    node_info.live_time_stamp = itn->second->m_live_time;
                    node_info.addr.ip = itn->second->m_peer_addr.get_ip();
                    node_info.addr.port = itn->second->m_peer_addr.get_port();
                    node_info.node_type = (int8_t)itn->second->m_node_type;
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }
            else if(req->flag == matrix::service_core::flag_global)
            {
                //case: not too many peers
                for (auto it = m_peer_candidates.begin(); it != m_peer_candidates.end(); ++it)
                {
                    matrix::service_core::cmd_peer_node_info node_info;
                    node_info.peer_node_id = (*it)->node_id;
                    node_info.live_time_stamp = 0;
                    node_info.net_st = (int8_t)(*it)->net_st;
                    node_info.addr.ip = (*it)->tcp_ep.address().to_string();
                    node_info.addr.port = (*it)->tcp_ep.port();
                    node_info.node_type = (int8_t)(*it)->node_type;
                    node_info.service_list.clear();
                    node_info.service_list.push_back(std::string("ai_training"));
                    cmd_resp->peer_nodes_list.push_back(std::move(node_info));
                }
            }

            TOPIC_MANAGER->publish<void>(typeid(matrix::service_core::cmd_get_peer_nodes_resp).name(), cmd_resp);

            return E_SUCCESS;
        }
    }
}
