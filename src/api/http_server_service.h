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

#include <vector>
#include "module.h"
#include "http_server.h"

#define HTTP_SERVER_SERVICE_MODULE                             "http_server_service_module"

namespace matrix
{
    namespace core
    {
        constexpr int DEFAULT_HTTP_SERVER_TIMEOUT = 30;
        constexpr size_t MAX_HEADERS_SIZE = 8192;/** Maximum size of http request (request line + headers) */

        constexpr unsigned int MAX_BODY_SIZE = 0x02000000;// max http body size

        class http_server_service : public module
        {
         public:
            http_server_service(const std::shared_ptr<http_request_event>& hreq_event) : m_hreq_event(hreq_event)
            {}

            ~http_server_service() = default;

         public:
            std::string module_name() const override  { return HTTP_SERVER_SERVICE_MODULE; }

            int32_t init(bpo::variables_map& options) override;

            int32_t start() override
            {
                if (!is_prohibit_rest()) {
                    start_http_server();
                }
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

            void on_http_request_event(struct evhttp_request* req);
         private:
            bool init_http_server();

            void start_http_server();

            void interrupt_http_server();

            void stop_http_server();

            int32_t load_rest_config(bpo::variables_map& options);

            bool is_prohibit_rest()
            { return m_rest_port == 0; }

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

         private:
            std::string m_rest_ip = DEFAULT_LOOPBACK_IP;
            uint16_t m_rest_port = 0;
            std::vector<evhttp_bound_socket*> m_bound_sockets;
            // libevent event loop
            struct event_base* m_event_base_ptr = nullptr;
            // HTTP server
            struct evhttp* m_event_http_ptr = nullptr;


            std::thread m_thread_http;
            std::future<bool> m_thread_result;

            std::shared_ptr<http_request_event> m_hreq_event;

        };

    }  // namespace core
}  // namespace matrix

#endif
