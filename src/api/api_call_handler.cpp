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
            TOPIC_MANAGER->subscribe(typeid(cmd_start_training_resp).name(), [this](std::shared_ptr<cmd_start_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_resp).name(), [this](std::shared_ptr<cmd_stop_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_start_multi_training_resp).name(), [this](std::shared_ptr<cmd_start_multi_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_resp).name(), [this](std::shared_ptr<cmd_list_training_resp> &rsp) {m_resp = rsp; m_wait->set(); });
            TOPIC_MANAGER->subscribe(typeid(cmd_get_peer_nodes_resp).name(), [this](std::shared_ptr<cmd_get_peer_nodes_resp> &rsp) {m_resp = rsp; m_wait->set(); });
        }

    }
}
