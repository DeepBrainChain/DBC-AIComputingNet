/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        : api_call_handler.h
* description    : api call handler for command line, json rpc
* date                  : 2018.03.25
* author             : Bruce Feng
**********************************************************************************/

#pragma once

#include "cmd_message.h"

#define LIST_ALL_TASKS            0
#define LIST_SPECIFIC_TASKS       1

namespace dbc {
    class api_call_handler {
    public:

        api_call_handler() :
                m_wait(std::make_shared<callback_wait<>>()) {
//                init_subscription();
        }

        ~api_call_handler() = default;

        void init_subscription();

        template<typename req_type, typename resp_type>
        std::shared_ptr<resp_type> invoke(std::shared_ptr<req_type> req) {

            // m_mutex
            //This is a global lock that affects concurrent performance and will be optimized in the next release
            //
            std::unique_lock<std::mutex> lock(m_mutex);

            //construct message
            m_session_id = id_generator::generate_session_id();
            req->header.__set_session_id(m_session_id);
            std::shared_ptr<message> msg = std::make_shared<message>();
            msg->set_name(typeid(req_type).name());
            msg->set_content(req);

            m_wait->reset();

            //publish
            TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);

            //synchronous wait for resp
            std::chrono::milliseconds DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS = std::chrono::milliseconds(
                    CONF_MANAGER->get_timer_dbc_request_in_millisecond() * 12 / 10);

            m_wait->wait_for(DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS);
            if (true == m_wait->flag()) {
                return std::dynamic_pointer_cast<resp_type>(m_resp);
            } else {
                LOG_DEBUG << "api_call_handler time out: " << msg->get_name();
                return nullptr;
            }

        }

    private:

        std::mutex m_mutex;

        std::shared_ptr<matrix::core::base> m_resp;

        std::shared_ptr<callback_wait<>> m_wait;

        std::string m_session_id;

    };

    extern std::unique_ptr<api_call_handler> g_api_call_handler;
}
