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

#define HTTP_SERVER_SERVICE_MODULE                             "http_server_service_module"

namespace matrix
{
    namespace core
    {
        constexpr int DEFAULT_HTTP_SERVER_TIMEOUT = 30;
        constexpr int DEFAULT_HTTP_THREADS = 4;
        constexpr int DEFAULT_HTTP_WORKQUEUE = 16;
        /** Maximum size of http request (request line + headers) */
        constexpr size_t MAX_HEADERS_SIZE = 8192;
        // max http body size
        constexpr unsigned int MAX_BODY_SIZE = 0x02000000;

        template <typename work_item> class work_queue;
        class http_closure;

        class http_server_service : public module
        {
        public:
            http_server_service() {}

            ~http_server_service() = default;

        public:
            std::string module_name() const override { return HTTP_SERVER_SERVICE_MODULE; }

            int32_t init(bpo::variables_map &options) override;

            int32_t start() override
            {
                if (!is_prohibit_rest())
                    start_http_server();
                return E_SUCCESS;
            }

            int32_t stop() override
            {
                if (!is_prohibit_rest())
                    interrupt_http_server();
                return E_SUCCESS;
            }

            int32_t exit() override
            {
                if (!is_prohibit_rest())
                    stop_http_server();
                return E_SUCCESS;
            }

            /** register handler for prefix */
            void register_http_handler(const std::string &prefix, bool exact_match, const http_request_handler &handler);
            /** Unregister handler for prefix */
            void unregister_http_handler(const std::string &prefix, bool exact_match);

            work_queue<http_closure>* get_work_queue_ptr() { return work_queue_ptr; }

            struct event_base* get_event_base_ptr() { return event_base_ptr; }

            std::vector<http_path_handler>& get_http_path_handler() { return path_handlers; }

        private:
            bool init_http_server();
            void start_http_server();
            void interrupt_http_server();
            void stop_http_server();

            int32_t load_rest_config(bpo::variables_map &options);

            bool is_prohibit_rest() { return rest_port == 0; }

            /** Bind HTTP server to specified addresses */
            bool http_bind_addresses(struct evhttp* http);

            /** Change logging level for libevent. */
            void update_http_server_logging(bool enable);

            /** libevent event log callback */
            static void libevent_log_cb(int severity, const char *msg);
            /** HTTP request callback */
            static void http_request_cb(struct evhttp_request* req, void* arg);
            /** Callback to reject HTTP requests after shutdown. */
            static void http_reject_request_cb(struct evhttp_request* req, void*);
            /** Event dispatcher thread */
            static bool thread_http_fun(struct event_base* base, struct evhttp* http);
            /** Simple wrapper to set thread name and run work queue */
            static void http_workqueue_run(work_queue<http_closure>* queue);
            /*modify thead name*/
            static void rename_thread(const char* name);

        private:
            std::string rest_ip = DEFAULT_LOOPBACK_IP;
            uint16_t rest_port = 0;

            std::vector<evhttp_bound_socket *> bound_sockets;
            work_queue<http_closure>* work_queue_ptr = nullptr;

            // libevent event loop
            struct event_base* event_base_ptr = nullptr;
            // HTTP server
            struct evhttp* event_http_ptr = nullptr;
            // Handlers for (sub)paths
            std::vector<http_path_handler> path_handlers;

            std::thread thread_http;
            std::future<bool> thread_result;
            std::vector<std::thread> thread_http_workers;
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
            http_work_item(std::unique_ptr<http_request> _req, const std::string &_path, const http_request_handler& _func):
                req(std::move(_req)), path(_path), func(_func)
            {
            }
            void operator()() override
            {
                func(req.get(), path);
            }

            std::unique_ptr<http_request> req;

        private:
            std::string path;
            http_request_handler func;
        };

        /** Simple work queue for distributing work over multiple threads.
         * Work items are simply callable objects.
         */
        template <typename work_item>
        class work_queue
        {
        public:
            explicit work_queue(size_t max_depth_) : running(true),
                                max_depth(max_depth_)
            {
            }
            /** Precondition: worker threads have all stopped (they have been joined).
             */
            ~work_queue()
            {
            }
            /** Enqueue a work item */
            bool enqueue(work_item* item)
            {
                std::unique_lock<std::mutex> lock(cs);
                if (queue.size() >= max_depth) {
                    return false;
                }
                queue.emplace_back(std::unique_ptr<work_item>(item));
                cond.notify_one();
                return true;
            }

            /** Thread function */
            void run()
            {
                while (true) {
                    std::unique_ptr<work_item> i;
                    {
                        std::unique_lock<std::mutex> lock(cs);
                        while (running && queue.empty())
                            cond.wait(lock);
                        if (!running)
                            break;
                        i = std::move(queue.front());
                        queue.pop_front();
                    }
                    (*i)();
                }
            }

            /** Interrupt and exit loops */
            void interrupt()
            {
                std::unique_lock<std::mutex> lock(cs);
                running = false;
                cond.notify_all();
            }

        private:
            /** Mutex protects entire object */
            std::mutex cs;
            std::condition_variable cond;
            std::deque<std::unique_ptr<work_item>> queue;
            bool running;
            size_t max_depth;
        };

    }  // namespace core
}  // namespace matrix

#endif
