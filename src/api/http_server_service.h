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
        constexpr int DEFAULT_HTTP_SERVER_TIMEOUT = 30;
        constexpr int DEFAULT_HTTP_THREADS = 4;
        constexpr int DEFAULT_HTTP_WORKQUEUE = 16;
/** Maximum size of http request (request line + headers) */
        constexpr size_t MAX_HEADERS_SIZE = 8192;
// max http body size
        constexpr unsigned int MAX_BODY_SIZE = 0x02000000;

        template<typename work_item>
            class work_queue;

        class http_closure;

        class http_server_service : public service_module
        {
         public:
            http_server_service() {}

            ~http_server_service() = default;

         public:
            std::string module_name() const override { return HTTP_SERVER_SERVICE_MODULE; }

            int32_t init(bpo::variables_map &options) override;

            int32_t start() override {
                if (!is_prohibit_rest()) {
                    start_http_server();
                }

                service_module::start();
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

            /** register handler for prefix */
            void register_http_handler(const std::string &prefix,
                                       bool exact_match,
                                       const http_request_handler &handler);

            /** Unregister handler for prefix */
            void unregister_http_handler(const std::string &prefix, bool exact_match);

            work_queue<http_closure> *get_work_queue_ptr() { return m_work_queue_ptr; }

            struct event_base *get_event_base_ptr() { return m_event_base_ptr; }

            std::vector<http_path_handler> &get_http_path_handler() { return m_path_handlers; }

         protected:
            virtual void init_invoker();
            virtual void init_subscription();
            virtual void init_timer();

         private:
            bool init_http_server();

            void start_http_server();

            void interrupt_http_server();

            void stop_http_server();

            int32_t load_rest_config(bpo::variables_map &options);

            bool is_prohibit_rest() { return m_rest_port==0; }

            /** Bind HTTP server to specified addresses */
            bool http_bind_addresses(struct evhttp *http);

            /** HTTP request callback */
            static void http_request_cb(struct evhttp_request *req, void *arg);

            /** Callback to reject HTTP requests after shutdown. */
            static void http_reject_request_cb(struct evhttp_request *req, void *);

            /** Event dispatcher thread */
            static bool thread_http_fun(struct event_base *base, struct evhttp *http);

            /** Simple wrapper to set thread name and run work queue */
            static void http_workqueue_run(work_queue<http_closure> *queue);

            /*modify thead name*/
            static void rename_thread(const char *name);

            void on_http_request_event(const http_request_handler &handler,
                                       std::shared_ptr<http_request> &hreq,
                                       std::string path);

            int32_t on_call_rsp_handler(std::shared_ptr<message> &msg);

            int32_t on_http_request_timeout_event(std::shared_ptr<core_timer> timer);

         private:
            std::string m_rest_ip = DEFAULT_LOOPBACK_IP;
            uint16_t m_rest_port = 0;

            std::vector<evhttp_bound_socket *> m_bound_sockets;
            work_queue<http_closure> *m_work_queue_ptr = nullptr;

            // libevent event loop
            struct event_base *m_event_base_ptr = nullptr;
            // HTTP server
            struct evhttp *m_event_http_ptr = nullptr;
            // Handlers for (sub)paths
            std::vector<http_path_handler> m_path_handlers;

            std::thread m_thread_http;
            std::future<bool> m_thread_result;
            std::vector<std::thread> m_thread_http_workers;

            std::map<std::string, response_call_handler> m_rsp_handlers;

        };

/** Event handler closure.
 */
        class http_closure
        {
         public:
            virtual void operator()() = 0;

            virtual ~http_closure() {}
        };

/** HTTP request work item */
        class http_work_item final : public http_closure
        {
         public:
            http_work_item(std::unique_ptr<http_request> _req,
                           const std::string &_path,
                           const http_request_handler &_func) : m_req(std::move(_req)), m_path(_path), m_func(_func) {
            }

            void operator()() override {
                // m_func(m_req, m_path);
            }

            std::unique_ptr<http_request> m_req;

         private:
            std::string m_path;
            http_request_handler m_func;
        };

/** Simple work queue for distributing work over multiple threads.
 * Work items are simply callable objects.
 */
        template<typename work_item>
            class work_queue
            {
             public:
                explicit work_queue(size_t max_depth_) : m_running(true), m_max_depth(max_depth_) {
                }

                /** Precondition: worker threads have all stopped (they have been joined).
                 */
                ~work_queue() {
                }

                /** Enqueue a work item */
                bool enqueue(work_item *item) {
                    std::unique_lock<std::mutex> lock(m_cs);
                    if (m_queue.size() >= m_max_depth) {
                        return false;
                    }
                    m_queue.emplace_back(std::unique_ptr<work_item>(item));
                    m_cond.notify_one();
                    return true;
                }

                /** Thread function */
                void run() {
                    while (true) {
                        std::unique_ptr<work_item> i;
                        {
                            std::unique_lock<std::mutex> lock(m_cs);
                            while (m_running && m_queue.empty()) {
                                m_cond.wait(lock);
                            }
                            if (!m_running) {
                                break;
                            }
                            i = std::move(m_queue.front());
                            m_queue.pop_front();
                        }
                        (*i)();
                    }
                }

                /** Interrupt and exit loops */
                void interrupt() {
                    std::unique_lock<std::mutex> lock(m_cs);
                    m_running = false;
                    m_cond.notify_all();
                }

             private:
                /** Mutex protects entire object */
                std::mutex m_cs;
                std::condition_variable m_cond;
                std::deque<std::unique_ptr<work_item>> m_queue;
                bool m_running;
                size_t m_max_depth;
            };

    }  // namespace core
}  // namespace matrix

#endif
