/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rest_api_service.cpp
* description       : Handle internal asynchronous calls to rest api
* date              : 2018.12.10
* author            : tazzhang
**********************************************************************************/

#include "rest_api_service.h"
#include "rest_handler.h"
#include "rpc_error.h"
#include "log.h"
#include "service_message_id.h"
#include "time_tick_notification.h"
#include <chrono>
#include <ctime>
#include "message.h"

using namespace std::chrono;
extern std::chrono::high_resolution_clock::time_point server_start_time;
//using namespace ai::dbc;

#define HTTP_REQUEST_TIMEOUT_EVENT   "http_request_timeout_event"
#define HTTP_REQUEST_KEY             "hreq_context"

namespace dbc {
    int32_t rest_api_service::init(bpo::variables_map &options) {
        const http_path_handler uri_prefixes[] = {
                {"/peers",        false, rest_peers},
                {"/stat",         false, rest_stat},
                {"/mining_nodes", false, rest_mining_nodes},
                {"/tasks",        false, rest_task},
                {"/config",       false, rest_config},
                {"",              true,  rest_api_version},
        };

        for (const auto &uri_prefixe : uri_prefixes) {
            m_path_handlers.emplace_back(REST_API_URI + uri_prefixe.m_prefix,
                                         uri_prefixe.m_exact_match,
                                         uri_prefixe.m_handler);
        }

        const response_msg_handler rsp_handlers[] = {
                {typeid(cmd_create_task_rsp).name(), on_cmd_create_task_rsp},
                {typeid(cmd_start_task_rsp).name(), on_cmd_start_task_rsp},
                {typeid(cmd_restart_task_rsp).name(), on_cmd_restart_task_rsp},
                {typeid(cmd_stop_task_rsp).name(), on_cmd_stop_task_rsp},
                {typeid(cmd_task_logs_rsp).name(), on_cmd_task_logs_rsp},
                {typeid(cmd_list_task_rsp).name(), on_cmd_list_task_rsp},
                {typeid(cmd_get_peer_nodes_rsp).name(), on_cmd_get_peer_nodes_rsp},
                {typeid(cmd_list_node_rsp).name(), on_list_node_rsp}
        };

        for (const auto &rsp_handler : rsp_handlers) {
            std::string name = rsp_handler.name;
            m_rsp_handlers[name] = rsp_handler.handler;
        }

        service_module::init(options);
        return E_SUCCESS;
    }

    void rest_api_service::init_timer() {
        m_timer_invokers[HTTP_REQUEST_TIMEOUT_EVENT] = std::bind(&rest_api_service::on_http_request_timeout_event,
                                                                 this, std::placeholders::_1);
    }

    void rest_api_service::init_invoker() {
        invoker_type invoker;
        BIND_MESSAGE_INVOKER(typeid(cmd_create_task_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_start_task_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_restart_task_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_stop_task_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_task_logs_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_list_task_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_get_peer_nodes_rsp).name(),&rest_api_service::on_call_rsp_handler);
        BIND_MESSAGE_INVOKER(typeid(cmd_list_node_rsp).name(),&rest_api_service::on_call_rsp_handler);
    }

    #define SUBSCRIBE_RESP_MSG(cmd)  \
    TOPIC_MANAGER->subscribe(typeid(cmd).name(),[this](std::shared_ptr<cmd> &rsp){  \
        std::shared_ptr<message> msg = std::make_shared<message>(); \
        msg->set_name(typeid(cmd).name()); \
        msg->set_content(rsp);\
        this->send(msg);\
    });

    void rest_api_service::init_subscription() {
        SUBSCRIBE_RESP_MSG(cmd_create_task_rsp)
        SUBSCRIBE_RESP_MSG(cmd_start_task_rsp);
        SUBSCRIBE_RESP_MSG(cmd_restart_task_rsp);
        SUBSCRIBE_RESP_MSG(cmd_stop_task_rsp);
        SUBSCRIBE_RESP_MSG(cmd_task_logs_rsp);
        SUBSCRIBE_RESP_MSG(cmd_list_task_rsp);
        SUBSCRIBE_RESP_MSG(cmd_get_peer_nodes_rsp);
        SUBSCRIBE_RESP_MSG(cmd_list_node_rsp);
    }

    void rest_api_service::on_http_request_event(std::shared_ptr<http_request> &hreq) {
        std::string str_uri = hreq->get_uri();
        if (check_invalid_http_request(hreq)) {
            LOG_ERROR << "the request is invalid,so reject it," << str_uri;
            return;
        }

        std::shared_ptr<message> req_msg = parse_http_request(hreq);
        if (nullptr == req_msg) {
            LOG_ERROR << "parse_http_request,there was an error," << str_uri;
            return;
        }

        post_cmd_request_msg(hreq, req_msg);
    }

    bool rest_api_service::check_invalid_http_request(std::shared_ptr<http_request> &hreq) {
        if (hreq->get_request_method() == http_request::UNKNOWN) {
            hreq->reply_comm_rest_err(HTTP_BADMETHOD, RPC_METHOD_DEPRECATED, "not support method");
            return true;
        }

        auto time_span_ms = duration_cast<milliseconds>(high_resolution_clock::now() - server_start_time);

        if (time_span_ms.count() < MIN_INIT_HTTP_SERVER_TIME) {

            hreq->reply_comm_rest_err(HTTP_OK, RPC_REQUEST_INTERRUPTED, "Request is interrupted.Try again later.");

            return true;
        }

        std::string str_uri = hreq->get_uri();
        if (str_uri.substr(0, REST_API_URI.size()) != REST_API_URI) {

            hreq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "The api version is invalid. Current "
                                                                            "version is " + std::string(REST_API_URI) + ".");
            return true;
        }

        return false;
    }

    std::shared_ptr<message> rest_api_service::parse_http_request(std::shared_ptr<http_request> &hreq) {
        std::string str_uri = hreq->get_uri();
        std::string path;
        bool match;
        std::vector<http_path_handler>::const_iterator i = m_path_handlers.begin();
        std::vector<http_path_handler>::const_iterator iend = m_path_handlers.end();
        for (; i != iend; ++i) {
            match = false;
            if (i->m_exact_match) {
                match = (str_uri == i->m_prefix);
            } else {
                match = (str_uri.substr(0, i->m_prefix.size()) == i->m_prefix);
            }

            if (match) {
                path = str_uri.substr(i->m_prefix.size());
                break;
            }
        }

        if (i == iend) {
            hreq->reply_comm_rest_err(HTTP_NOTFOUND, RPC_METHOD_NOT_FOUND, "not support RESTful api");
            return nullptr;
        }
        return i->m_handler(hreq, path);
    }

    void rest_api_service::post_cmd_request_msg(std::shared_ptr<http_request> &hreq, std::shared_ptr<message> &req_msg) {
        std::string str_uri = hreq->get_uri();
        std::string session_id = id_generator::generate_session_id();
        std::shared_ptr<msg_base> req_content = req_msg->get_content();
        req_content->header.__set_session_id(session_id);

        std::shared_ptr<http_request_context> hreq_context = std::make_shared<http_request_context>();
        hreq_context->m_hreq = hreq;
        hreq_context->m_req_msg = req_msg;

        uint32_t timer_id = INVALID_TIMER_ID;
        std::shared_ptr<service_session> session = nullptr;

        do {
            if (get_session_count() >= MAX_SESSION_COUNT) {

                LOG_ERROR << "str_uri:" << str_uri << ";m_sessions.size() exceed size";

                hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "session pool is full.");

                return;
            }

            timer_id = add_timer(HTTP_REQUEST_TIMEOUT_EVENT,
                                 MAX_WAIT_HTTP_RESPONSE_TIME,
                                 ONLY_ONE_TIME,
                                 session_id);
            if (INVALID_TIMER_ID == timer_id) {
                LOG_ERROR << "str_uri:" << str_uri << ";add_timer failed";

                hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "System error.");

                return;
            }

            session = std::make_shared<service_session>(timer_id, session_id);
            variable_value val;
            val.value() = hreq_context;
            session->get_context().add(HTTP_REQUEST_KEY, val);

            int32_t ret = this->add_session(session_id, session);
            if (E_SUCCESS != ret) {
                remove_timer(timer_id);

                LOG_ERROR << "str_uri:" << str_uri << ";add_session failed";

                hreq->reply_comm_rest_err(HTTP_OK, RPC_RESPONSE_ERROR, "Session error..");

                return;
            }

            LOG_INFO << "str_uri:" << str_uri << ";add_session:" << session_id << ";msg_name:"
                     << req_msg->get_name();
        } while (0);

        TOPIC_MANAGER->publish<int32_t>(req_msg->get_name(), req_msg);
    }

    int32_t rest_api_service::on_call_rsp_handler(std::shared_ptr<message> &msg) {
        int32_t ret_code = E_DEFAULT;

        std::shared_ptr<service_session> session = nullptr;

        do {
            const std::string &name = msg->get_name();
            const std::string &session_id = msg->get_content()->header.session_id;
            if (session_id.empty()) {
                //on_call_rsp_handler | rsp name: N2ai3dbc13cmd_show_respE,but get null session_id
                LOG_ERROR << "rsp name: " << name << ",but get null session_id";
                ret_code = E_NULL_POINTER;
                break;

            }

            session = pop_session(session_id);
            if (nullptr == session) {

                LOG_DEBUG << "rsp name: " << name << ",but cannot find  session_id:" << session_id;
                ret_code = E_NULL_POINTER;
                break;
            }

            try {
                remove_timer(session->get_timer_id());
                variables_map &vm = session->get_context().get_args();

                if (0 == vm.count(HTTP_REQUEST_KEY)) {
                    LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id << ",but get null hreq_key";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                std::shared_ptr<http_request_context> hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<
                        http_request_context>>();

                if (nullptr == hreq_context) {
                    LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id
                              << ",but get null hreq_context";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                auto it = m_rsp_handlers.find(name);
                if (it == m_rsp_handlers.end()) {
                    LOG_ERROR << "rsp name: " << name << ",session_id:" << session_id
                              << ",but get null rsp_handler";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                LOG_INFO << "rsp name: " << name << ",session_id:" << session_id << ",and call rsp_handler";
                auto func = it->second;
                func(hreq_context, msg);
                ret_code = E_SUCCESS;
            } catch (system_error &e) {
                LOG_ERROR << "error: " << e.what();
            } catch (...) {
                LOG_ERROR << "error: Unknown error.";
            }
        } while (0);

        return ret_code;

    }

    int32_t rest_api_service::on_http_request_timeout_event(std::shared_ptr<core_timer> timer) {

        int32_t ret_code = E_SUCCESS;
        std::string session_id;
        std::shared_ptr<service_session> session = nullptr;

        do {
            if (nullptr == timer) {
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

            session = pop_session(session_id);
            if (nullptr == session) {
                LOG_ERROR << "cannot find  session_id:" << session_id;
                ret_code = E_NULL_POINTER;
                break;
            }

            try {
                variables_map &vm = session->get_context().get_args();
                assert(vm.count(HTTP_REQUEST_KEY) > 0);
                std::shared_ptr<http_request_context> hreq_context = vm[HTTP_REQUEST_KEY].as<std::shared_ptr<
                        http_request_context>>();
                if (nullptr == hreq_context) {
                    LOG_ERROR << "session_id:" << session_id << ",but get null hreq_context";
                    ret_code = E_NULL_POINTER;
                    break;
                }

                hreq_context->m_hreq->reply_comm_rest_err(HTTP_INTERNAL, RPC_RESPONSE_TIMEOUT, "Internal call "
                                                                                               "timeout...");
            } catch (system_error &e) {
                LOG_ERROR << "error: " << e.what();
            } catch (...) {
                LOG_ERROR << "error: Unknown error.";
            }

        } while (0);

        return ret_code;

    }

    int32_t rest_api_service::get_session_count() {
        std::unique_lock<std::mutex> lock(m_session_lock);
        return m_sessions.size();
    }

    int32_t rest_api_service::get_startup_time() {
        auto time_span_ms = duration_cast<milliseconds>(high_resolution_clock::now() - server_start_time);
        int32_t fractional_seconds = (int32_t) (time_span_ms.count() / 1000);
        return fractional_seconds;
    }

    int32_t rest_api_service::on_invoke(std::shared_ptr<message> &msg) {
        //timer point notification trigger timer process
        if (msg->get_name() == TIMER_TICK_NOTIFICATION) {
            std::shared_ptr<time_tick_notification> content = std::dynamic_pointer_cast<time_tick_notification>(
                    msg->get_content());
            assert(nullptr != content);

            std::unique_lock<std::mutex> lock(m_timer_lock);
            return m_timer_manager->process(content->time_tick);

        } else {
            return service_msg(msg);
        }
    }

    uint32_t rest_api_service::add_timer(std::string name, uint32_t period, uint64_t repeat_times, const std::string &session_id) {
        std::unique_lock<std::mutex> lock(m_timer_lock);
        return m_timer_manager->add_timer(name, period, repeat_times, session_id);
    }

    void rest_api_service::remove_timer(uint32_t timer_id) {
        std::unique_lock<std::mutex> lock(m_timer_lock);
        m_timer_manager->remove_timer(timer_id);
    }

    int32_t rest_api_service::add_session(std::string session_id, std::shared_ptr<service_session> session) {
        std::unique_lock<std::mutex> lock(m_session_lock);
        auto it = m_sessions.find(session_id);
        if (it != m_sessions.end()) {
            return E_DEFAULT;
        }

        m_sessions.insert({session_id, session});
        return E_SUCCESS;
    }

    std::shared_ptr<service_session> rest_api_service::pop_session(const std::string& session_id) {
        std::shared_ptr<service_session> session = nullptr;

        std::unique_lock<std::mutex> lock(m_session_lock);
        auto it = m_sessions.find(session_id);
        if (it == m_sessions.end()) {
            return nullptr;
        }

        session = it->second;
        m_sessions.erase(session_id);
        return session;
    }
}
