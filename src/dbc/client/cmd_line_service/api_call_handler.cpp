/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        : api_call_handler.cpp
* description    : api call handler for command line, json rpc
* date                  : 2018.03.27
* author             :  Bruce Feng
**********************************************************************************/

#include "matrix_coder.h"
#include "api_call_handler.h"
#include "service_message_id.h"

//
//the session id of the request and response must match,then wait_event can be set()
//
#define SUBSCRIBE_RESP_MSG(cmd)  TOPIC_MANAGER->subscribe(typeid(cmd).name(),\
[this](std::shared_ptr<cmd> &rsp){if( rsp->header.session_id.empty() ||rsp->header.session_id == m_session_id)\
{m_resp = rsp;m_wait->set();}});

namespace dbc {
    std::unique_ptr<api_call_handler> g_api_call_handler(new api_call_handler());

    void api_call_handler::init_subscription() {
        SUBSCRIBE_RESP_MSG(cmd_start_training_resp);
        SUBSCRIBE_RESP_MSG(cmd_stop_training_resp);
        SUBSCRIBE_RESP_MSG(cmd_list_training_resp);
        SUBSCRIBE_RESP_MSG(cmd_get_peer_nodes_resp);
        SUBSCRIBE_RESP_MSG(cmd_logs_resp);
        SUBSCRIBE_RESP_MSG(cmd_show_resp);
        SUBSCRIBE_RESP_MSG(cmd_task_clean_resp);
    }
}
