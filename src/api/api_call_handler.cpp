/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        api_call_handler.cpp
* description    api call handler for command line, json rpc
* date                  : 2018.03.27
* author            Bruce Feng
**********************************************************************************/

#include "matrix_coder.h"
#include "api_call_handler.h"
#include "matrix_coder.h"
#include "service_message_id.h"


namespace ai
{
    namespace dbc
    {

        void api_call_handler::init_subscription()
        {
            TOPIC_MANAGER->subscribe(typeid(cmd_start_training_resp).name(), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_resp).name(), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_start_multi_training_resp).name(), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_resp).name(), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });

            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_stop_training_req(msg);});
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_list_training_req(msg);});
            TOPIC_MANAGER->subscribe(typeid(cmd_get_peer_nodes_req).name(), [this](const std::shared_ptr<message> &msg) {return this->on_cmd_get_peer_nodes_req(msg); });
            TOPIC_MANAGER->subscribe(typeid(cmd_get_peer_nodes_resp).name(), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
        }

        int32_t api_call_handler::on_cmd_stop_training_req(const std::shared_ptr<message> &msg) const
        {
            const std::string &task_id = std::dynamic_pointer_cast<cmd_stop_training_req>(msg->get_content())->task_id;
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = STOP_TRAINING_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;
            //body
            req_content->body.task_id = task_id;

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
            CONNECTION_MANAGER->broadcast_message(req_msg);

            //there's no reply, so public resp directly
            std::shared_ptr<ai::dbc::cmd_stop_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_stop_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t api_call_handler::on_cmd_list_training_req(const std::shared_ptr<message> &msg) const
        {
            auto cmd_req = std::dynamic_pointer_cast<cmd_list_training_req>(msg->get_content());
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::list_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = LIST_TRAINING_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;
            //body
            if (cmd_req->list_type == 1) {
                req_content->body.task_list.assign(cmd_req->task_list.begin(), cmd_req->task_list.end());
            }

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
            CONNECTION_MANAGER->broadcast_message(req_msg);
            return E_SUCCESS;
        }

        int32_t api_call_handler::on_cmd_get_peer_nodes_req(const std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::get_peer_nodes_req> req_content = std::make_shared<matrix::service_core::get_peer_nodes_req>();
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = P2P_GET_PEER_NODES_REQ;
            req_content->header.check_sum = 0;//todo ...
            req_content->header.session_id = 0;
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            req_msg->set_name(P2P_GET_PEER_NODES_REQ);
            req_msg->set_content(std::dynamic_pointer_cast<base> (req_content));
            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }


    }
}
