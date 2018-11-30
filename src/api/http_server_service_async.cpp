/********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : http_server_service_async.cpp
* description       : Dealing exclusively with asynchronous events in http
* date              : 2018.11.29
* author            : tazzhang
**********************************************************************************/

#include "http_server_service.h"
#include "rpc_error.h"

using namespace ai::dbc;

//
//Turn the response package into a message and send it to the queue
//

#define SUBSCRIBE_RESP_MSG(cmd)  TOPIC_MANAGER->subscribe(typeid(cmd).name(),[this](std::shared_ptr<cmd> &rsp){  \
std::shared_ptr<message> msg = std::make_shared<message>(); \
msg->set_name(typeid(cmd).name()); \
msg->set_content(rsp);\
send(msg);\
}); \
\

//
//Bind Async Call Handler of Response Msg
//
#define BIND_RSP_HANDLER(cmd)  BIND_MESSAGE_INVOKER(typeid(cmd).name(),&http_server_service::on_call_rsp_handler);

#define HTTP_REQUEST_TIMEOUT_EVENT   "http_request_timeout_event"
#define HTTP_REQUEST_KEY             "hreq_context"

namespace matrix
{
    namespace core
    {
        void http_server_service::init_subscription() {
            SUBSCRIBE_RESP_MSG(cmd_start_training_resp);
            SUBSCRIBE_RESP_MSG(cmd_stop_training_resp);
            SUBSCRIBE_RESP_MSG(cmd_start_multi_training_resp);
            SUBSCRIBE_RESP_MSG(cmd_list_training_resp);
            SUBSCRIBE_RESP_MSG(cmd_get_peer_nodes_resp);
            SUBSCRIBE_RESP_MSG(cmd_logs_resp);
            SUBSCRIBE_RESP_MSG(cmd_show_resp);
            SUBSCRIBE_RESP_MSG(cmd_ps_resp);
            SUBSCRIBE_RESP_MSG(cmd_task_clean_resp);
        }

        void http_server_service::init_invoker() {
            invoker_type invoker;
            BIND_RSP_HANDLER(cmd_start_training_resp);
            BIND_RSP_HANDLER(cmd_stop_training_resp);
            BIND_RSP_HANDLER(cmd_start_multi_training_resp);
            BIND_RSP_HANDLER(cmd_list_training_resp);
            BIND_RSP_HANDLER(cmd_get_peer_nodes_resp);
            BIND_RSP_HANDLER(cmd_logs_resp);
            BIND_RSP_HANDLER(cmd_show_resp);
            BIND_RSP_HANDLER(cmd_ps_resp);
            BIND_RSP_HANDLER(cmd_task_clean_resp);

        }

        void http_server_service::init_timer() {
            m_timer_invokers[HTTP_REQUEST_TIMEOUT_EVENT] = std::bind(&http_server_service::on_http_request_timeout_event,
                                                                     this,
                                                                     std::placeholders::_1);

        }

        ///
        ///The worker thread retrieves a message to invoke this procedure for processing
        ///
        int32_t http_server_service::on_call_rsp_handler(std::shared_ptr<message>& msg) {
            int32_t ret_code = E_DEFAULT;

            std::shared_ptr<service_session> session = nullptr;

            do {
                const std::string& name = msg->get_name();
                const std::string& session_id = msg->get_content()->header.session_id;
                if (session_id.empty()) {
                    //on_call_rsp_handler | rsp name: N2ai3dbc13cmd_show_respE,but get null session_id
                    LOG_ERROR << "rsp name: " << name << ",but get null session_id";
                    ret_code = E_NULL_POINTER;
                    break;

                }

                session = get_session(session_id);
                if (nullptr==session) {

                    LOG_ERROR << "rsp name: " << name << ",but cannot find  session_id:" << session_id;
                    ret_code = E_NULL_POINTER;
                    break;
                }

                try {
                    variables_map& vm = session->get_context().get_args();

                    if(0 == vm.count(HTTP_REQUEST_KEY))
                    {
                        LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id
                                  << ",but get null hreq_key";
                        ret_code = E_NULL_POINTER;
                        break;
                    }

                    std::shared_ptr<http_request_context> hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<
                        http_request_context>>();

                    if (nullptr==hreq_context) {
                        LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id
                                  << ",but get null hreq_context";
                        ret_code = E_NULL_POINTER;
                        break;
                    }

                    auto it = m_rsp_handlers.find(name);
                    if (it==m_rsp_handlers.end()) {
                        LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id
                                  << ",but get null rsp_handler";
                        ret_code = E_NULL_POINTER;
                        break;
                    }

                    LOG_INFO << "rsp name: " << name << ",session_id:" << session_id << ",and call rsp_handler";
                    auto func = it->second;
                    func(hreq_context, msg);
                    ret_code = E_SUCCESS;
                } catch (system_error& e) {
                    LOG_ERROR << "error: " << e.what();
                } catch (...) {
                    LOG_ERROR << "error: Unknown error.";
                }
            } while (0);

            if (nullptr!=session) {
                this->remove_timer(session->get_timer_id());

                session->clear();
                this->remove_session(session->get_session_id());
            }

            return ret_code;

        }

        /// on_http_request_event
        /// Converts the http request to a dbc protocol request and
        ///     then distributes the request to the specified thread
        void http_server_service::on_http_request_event(const http_request_handler& handler,
                                                        std::shared_ptr<http_request>& hreq,
                                                        std::string path) {
            std::shared_ptr<message> req_msg = handler(hreq, path);
            if (nullptr==req_msg) {
                //
                // The request is processed directly in last handler(),
                // and does not need to be forwarded to another service
                //
                return;
            }

            std::string session_id = id_generator().generate_session_id();
            std::shared_ptr<msg_base> req_content = req_msg->get_content();
            req_content->header.__set_session_id(session_id);

            std::shared_ptr<http_request_context> hreq_context = std::make_shared<http_request_context>();
            hreq_context->m_hreq = hreq;
            hreq_context->m_req_msg = req_msg;

            uint32_t timer_id = INVALID_TIMER_ID;
            std::shared_ptr<service_session> session = nullptr;

            timer_id = add_timer(HTTP_REQUEST_TIMEOUT_EVENT, MAX_WAIT_HTTP_RESPONSE_TIME, ONLY_ONE_TIME, session_id);
            if (INVALID_TIMER_ID==timer_id) {
                LOG_ERROR << "path:" << path << ";add_timer failed";

                hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "System error.");

                return;
            }

            session = std::make_shared<service_session>(timer_id, session_id);
            variable_value val;
            val.value() = hreq_context;
            session->get_context().add(HTTP_REQUEST_KEY, val);

            int32_t ret = this->add_session(session_id, session);
            if (E_SUCCESS!=ret) {
                remove_timer(timer_id);

                LOG_ERROR << "path:" << path << ";add_session failed";

                hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "Session error..");

                return;
            }

            LOG_ERROR << "path:" << path << ";add_session:" << session_id << ";msg_name:" << req_msg->get_name();
            TOPIC_MANAGER->publish<int32_t>(req_msg->get_name(), req_msg);
        }

        int32_t http_server_service::on_http_request_timeout_event(std::shared_ptr<core_timer> timer) {

            int32_t ret_code = E_SUCCESS;
            std::string session_id;
            std::shared_ptr<service_session> session = nullptr;

            do {
                if (nullptr==timer) {
                    LOG_ERROR << "null ptr of timer.";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                session_id = timer->get_session_id();
                if (session_id.empty()) {
                    LOG_ERROR << "get null session_id";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                session = get_session(session_id);
                if (nullptr==session) {
                    LOG_ERROR << "cannot find  session_id:" << session_id;
                    ret_code = E_NULL_POINTER;
                    break;
                }

                try {
                    variables_map& vm = session->get_context().get_args();
                    assert(vm.count(HTTP_REQUEST_KEY) > 0);
                    std::shared_ptr<http_request_context> hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<
                        http_request_context>>();
                    if (nullptr==hreq_context) {
                        LOG_ERROR << "session_id:" << session_id << ",but get null hreq_context";
                        ret_code = E_NULL_POINTER;
                        break;
                    }

                    hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, RPC_RESPONSE_TIMEOUT, "Internal call "
                                                                                                   "timeout...");
                } catch (system_error& e) {
                    LOG_ERROR << "error: " << e.what();
                } catch (...) {
                    LOG_ERROR << "error: Unknown error.";
                }

            } while (0);

            if (nullptr!=session) {
                session->clear();
                remove_session(session_id);
            }

            return ret_code;

        }

    }
}