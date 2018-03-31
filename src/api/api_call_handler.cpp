/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºapi_call_handler.h
* description    £ºapi call handler for command line, json rpc
* date                  : 2018.03.27
* author            £º
**********************************************************************************/

#include "api_call_handler.h"


namespace ai
{
    namespace dbc
    {

        void api_call_handler::init_subscription()
        {
            TOPIC_MANAGER->subscribe(GET_TYPE_NAME(cmd_start_training_resp), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(GET_TYPE_NAME(cmd_stop_training_resp), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(GET_TYPE_NAME(cmd_start_multi_training_resp), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(GET_TYPE_NAME(cmd_list_training_resp), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
            TOPIC_MANAGER->subscribe(GET_TYPE_NAME(cmd_get_peer_nodes_resp), [this](std::shared_ptr<message> msg) {m_resp = msg; m_wait->set(); });
        }

    }

}
