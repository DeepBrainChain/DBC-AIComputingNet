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
#include <stdint.h>
#include <functional>

#include "module.h"
#include "api_call_handler.h"
#include "http_server.h"
#include "timer.h"

#define HTTP_SERVER_SERVICE_MODULE                             "http_server_service_module"

namespace matrix
{
    namespace core
    {
        constexpr int MIN_INIT_HTTP_SERVER_TIME = 5000;//ms
        constexpr int MAX_WAIT_HTTP_RESPONSE_TIME = 25000;//ms
        constexpr int MAX_SESSION_COUNT = 1024;
        constexpr int DEFAULT_HTTP_SERVER_TIMEOUT = 30;
        constexpr int DEFAULT_HTTP_THREADS = 4;
        constexpr int DEFAULT_HTTP_WORKQUEUE = 16;
/** Maximum size of http request (request line + headers) */
        constexpr size_t MAX_HEADERS_SIZE = 8192;
// max http body size
        constexpr unsigned int MAX_BODY_SIZE = 0x02000000;

        class http_server_service : public service_module
        {
         public:
            http_server_service()
            {}

            ~http_server_service() = default;

         public:
            std::string module_name() const override{ return HTTP_SERVER_SERVICE_MODULE; }

            int32_t init(bpo::variables_map& options) override;

            int32_t start() override
            {
                if (!is_prohibit_rest()) {
                    start_http_server();
                }

                service_module::start();
                return E_SUCCESS;
            }

            int32_t stop() override
            {
                if (!is_prohibit_rest()) {
                    interrupt_http_server();
                }
                return E_SUCCESS;
            }

            int32_t exit() override
            {
                if (!is_prohibit_rest()) {
                    stop_http_server();
                }
                return E_SUCCESS;
            }

            /** register handler for prefix */
            void register_http_handler(const std::string& prefix,
                                       bool exact_match,
                                       const http_request_handler& handler);

            /** Unregister handler for prefix */
            void unregister_http_handler(const std::string& prefix, bool exact_match);

            struct event_base* get_event_base_ptr(){ return m_event_base_ptr; }

            std::vector<http_path_handler>& get_http_path_handler(){ return m_path_handlers; }

         protected:
            virtual void init_invoker();
            virtual void init_subscription();
            virtual void init_timer();

         private:
            bool init_http_server();

            void start_http_server();

            void interrupt_http_server();

            void stop_http_server();

            int32_t load_rest_config(bpo::variables_map& options);

            bool is_prohibit_rest(){ return m_rest_port == 0; }

            /** Bind HTTP server to specified addresses */
            bool http_bind_addresses(struct evhttp* http);

            /** HTTP request callback */
            static void http_request_cb(struct evhttp_request* req, void* arg);

            /** Callback to reject HTTP requests after shutdown. */
            static void http_reject_request_cb(struct evhttp_request* req, void*);

            /** Event dispatcher thread */
            static bool thread_http_fun(struct event_base* base, struct evhttp* http);

            /*modify thead name*/
            static void rename_thread(const char* name);

            void on_http_request_event(const http_request_handler& handler,
                                       std::shared_ptr<http_request>& hreq,
                                       std::string path);

            int32_t on_call_rsp_handler(std::shared_ptr<message>& msg);

            int32_t on_http_request_timeout_event(std::shared_ptr<core_timer> timer);

         private:
            std::string m_rest_ip = DEFAULT_LOOPBACK_IP;
            uint16_t m_rest_port = 0;
            std::vector<evhttp_bound_socket*> m_bound_sockets;
            // libevent event loop
            struct event_base* m_event_base_ptr = nullptr;
            // HTTP server
            struct evhttp* m_event_http_ptr = nullptr;
            // Handlers for (sub)paths
            std::vector<http_path_handler> m_path_handlers;
            std::map<std::string, response_call_handler> m_rsp_handlers;

            std::thread m_thread_http;
            std::future<bool> m_thread_result;

            std::mutex m_mutex;

        };

    }  // namespace core
}  // namespace matrix

#endif
