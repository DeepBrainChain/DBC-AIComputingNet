/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : http_server_service.h
* description       : http server service manager
* date              : 2018.11.9
* author            : tower
**********************************************************************************/

#ifndef HTTP_SERVER_SERVICE_H
#define HTTP_SERVER_SERVICE_H

#include <memory>
#include <string>

#include <utility>
#include <vector>
#include "module.h"
#include "http_server.h"

#define HTTP_SERVER_SERVICE_MODULE                             "http_server_service_module"

namespace dbc {
    constexpr int DEFAULT_HTTP_SERVER_TIMEOUT = 30;
    constexpr size_t MAX_HEADERS_SIZE = 8192;    // Maximum size of http request (request line + headers)
    constexpr unsigned int MAX_BODY_SIZE = 0x02000000;  // max http body size

    class http_server_service : public module {
    public:
        explicit http_server_service(std::shared_ptr<http_request_event> hreq_event) : m_http_req_event(std::move(hreq_event)) {}

        ~http_server_service() override = default;

        std::string module_name() const override { return HTTP_SERVER_SERVICE_MODULE; }

        int32_t init(bpo::variables_map &options) override;

        int32_t start() override {
            if (!is_prohibit_rest()) {
                start_http_server();
            }
            return E_SUCCESS;
        }

        int32_t stop() override {
            if (!is_prohibit_rest()) {
                interrupt_http_server();
            }
            return E_SUCCESS;
        }

        int32_t exit() override {
            if (!is_prohibit_rest()) {
                stop_http_server();
            }
            return E_SUCCESS;
        }



    private:
        int32_t load_rest_config(bpo::variables_map &options);

        inline bool is_prohibit_rest() const {
            return m_listen_port == 0;
        }

        bool init_http_server();

        bool http_bind_addresses(struct evhttp *http);

        static void http_request_cb(struct evhttp_request *req, void *arg);

        void on_http_request_event(struct evhttp_request *req);

        void start_http_server();

        static bool thread_http_fun(struct event_base *base, struct evhttp *http);

        static void rename_thread(const char *name);

        void stop_http_server();

        void interrupt_http_server();

        static void http_reject_request_cb(struct evhttp_request *req, void *);










    private:
        std::string m_listen_ip = "127.0.0.1";
        uint16_t m_listen_port = 0;

        std::shared_ptr<http_request_event> m_http_req_event;
        struct event_base *m_event_base = nullptr;
        struct evhttp *m_event_http = nullptr;
        std::vector<evhttp_bound_socket *> m_bound_sockets;

        std::thread m_thread_http;
        std::future<bool> m_thread_result;
    };
}

#endif
