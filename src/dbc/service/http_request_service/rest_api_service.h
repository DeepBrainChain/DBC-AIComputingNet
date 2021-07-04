/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rest_api_service.h
* description       : Handle internal asynchronous calls to rest api
* date              : 2018.12.10
* author            : tazzhang
**********************************************************************************/

#ifndef REST_API_SERVICE_H
#define REST_API_SERVICE_H

#include <string>
#include <map>
#include "service_module.h"
#include "http_server.h"
#include "timer.h"

#define REST_API_SERVICE_MODULE                             "rest_api_service"

namespace dbc {
    constexpr int MIN_INIT_HTTP_SERVER_TIME = 5000;//ms
    constexpr int MAX_WAIT_HTTP_RESPONSE_TIME = 30000;//ms
    constexpr int MAX_SESSION_COUNT = 1024;

    class rest_api_service : public service_module, public dbc::network::http_request_event {
    public:
        rest_api_service() = default;

        ~rest_api_service() override = default;

        int32_t init(bpo::variables_map &options) override;

        std::string module_name() const override { return REST_API_SERVICE_MODULE; }

        int32_t get_session_count();

        int32_t get_startup_time();

    protected:
        void init_timer() override;

        void init_invoker() override;

        void init_subscription() override;

        void on_http_request_event(std::shared_ptr<dbc::network::http_request> &hreq) override;

        bool check_invalid_http_request(std::shared_ptr<dbc::network::http_request> &hreq);

        std::shared_ptr<dbc::network::message> parse_http_request(std::shared_ptr<dbc::network::http_request> &hreq);

        void post_cmd_request_msg(std::shared_ptr<dbc::network::http_request> &hreq, std::shared_ptr<dbc::network::message> &req_msg);

        int32_t on_call_rsp_handler(std::shared_ptr<dbc::network::message> &msg);

        int32_t on_http_request_timeout_event(std::shared_ptr<core_timer> timer);

        int32_t on_invoke(std::shared_ptr<dbc::network::message> &msg) override;

        //default: ULLONG_MAX  DEFAULT_STRING
        uint32_t add_timer(std::string name, uint32_t period, uint64_t repeat_times, const std::string &session_id) override;

        void remove_timer(uint32_t timer_id) override;

        int32_t add_session(std::string session_id, std::shared_ptr<service_session> session) override;

        std::shared_ptr<service_session> pop_session(const std::string& session_id);

    private:
        std::mutex m_timer_lock;
        std::mutex m_session_lock;
        std::vector<dbc::network::http_path_handler> m_path_handlers;
        std::map<std::string, dbc::network::response_call_handler> m_rsp_handlers;
    };
}

#endif
