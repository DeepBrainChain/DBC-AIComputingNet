/********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : http_server_service.cpp
* description       : http server service manager
* date              : 2018.11.9
* author            : tower
**********************************************************************************/

#include <assert.h>
#include <future>

#include <event2/thread.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/http.h>

#include "ip_validator.h"
#include "port_validator.h"
#include "server.h"
#include "log.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "rpc_error.h"
#include "http_client.h"
#include "http_server.h"
#include "http_server_service.h"

namespace matrix
{
    namespace core
    {
        int32_t http_server_service::init(bpo::variables_map &options)
        {
            int32_t ret = load_rest_config(options);
            if (ret != E_SUCCESS)
            {
                LOG_ERROR << "http server service load config error";
                return ret;
            }

            // prohibit http server if port is 0
            if (is_prohibit_rest())
                return E_SUCCESS;

            if (!init_http_server())
                return E_EXIT_FAILURE;

            return E_SUCCESS;
        }

        bool http_server_service::init_http_server()
        {
            // todo : support acl list in futurre

            // Redirect libevent's logging to our own log
            event_set_log_callback(&http_server_service::libevent_log_cb);
            // Update libevent's log handling. Returns false if our version of
            // libevent doesn't support debug logging, in which case we should
            // clear the BCLog::LIBEVENT flag.
            // default libevent log close,libevent log open if debug mode
            update_http_server_logging(0);
        #ifdef WIN32
            evthread_use_windows_threads();
        #else
            evthread_use_pthreads();
        #endif

            raii_event_base base_ctr = obtain_event_base();

            /* Create a new evhttp object to handle requests. */
            raii_evhttp http_ctr = obtain_evhttp(base_ctr.get());
            struct evhttp* http = http_ctr.get();
            if (!http) {
                LOG_ERROR << "couldn't create evhttp. Exiting.";
                return false;
            }

            evhttp_set_timeout(http, DEFAULT_HTTP_SERVER_TIMEOUT);
            evhttp_set_max_headers_size(http, MAX_HEADERS_SIZE);
            evhttp_set_max_body_size(http, MAX_BODY_SIZE);
            evhttp_set_gencb(http, http_server_service::http_request_cb, this);

            if (!http_bind_addresses(http)) {
                LOG_ERROR << "Unable to bind any endpoint for RPC server";
                return false;
            }

            work_queue_ptr = new work_queue<http_closure>(DEFAULT_HTTP_WORKQUEUE);
            // transfer ownership to eventBase/HTTP via .release()
            event_base_ptr = base_ctr.release();
            event_http_ptr = http_ctr.release();
            return true;
        }

        void http_server_service::start_http_server()
        {
            LOG_DEBUG << "Starting HTTP server";

            std::packaged_task<bool(event_base*, evhttp*)> task(thread_http_fun);
            thread_result = task.get_future();
            thread_http = std::thread(std::move(task), event_base_ptr, event_http_ptr);

            for (int i = 0; i < DEFAULT_HTTP_THREADS; i++) {
                thread_http_workers.emplace_back(http_server_service::http_workqueue_run, work_queue_ptr);
            }

            // register callback function

            // register_http_handler("/", false, rest_tx);
        }

        void http_server_service::interrupt_http_server()
        {
            LOG_INFO << "Interrupting HTTP server";
            if (event_http_ptr) {
                // Unlisten sockets
                for (evhttp_bound_socket *socket : bound_sockets) {
                    evhttp_del_accept_socket(event_http_ptr, socket);
                }
                // Reject requests on current connections
                evhttp_set_gencb(event_http_ptr, http_reject_request_cb, nullptr);
            }
            if (work_queue_ptr)
                work_queue_ptr->interrupt();
        }

        void http_server_service::stop_http_server()
        {
            LOG_INFO << "Stopping HTTP server";

            if (work_queue_ptr) {
                LOG_DEBUG << "Waiting for HTTP worker threads to exit";
                for (auto& thread : thread_http_workers) {
                    thread.join();
                }
                thread_http_workers.clear();
                delete work_queue_ptr;
                work_queue_ptr = nullptr;
            }
            if (event_base_ptr) {
                LOG_DEBUG << "Waiting for HTTP event thread to exit";
                // Exit the event loop as soon as there are no active events.
                event_base_loopexit(event_base_ptr, nullptr);
                // Give event loop a few seconds to exit (to send back last RPC responses), then break it
                // Before this was solved with event_base_loopexit, but that didn't work as expected in
                // at least libevent 2.0.21 and always introduced a delay. In libevent
                // master that appears to be solved, so in the future that solution
                // could be used again (if desirable).
                if (thread_result.valid() && thread_result.wait_for(std::chrono::milliseconds(2000)) == std::future_status::timeout) {
                    LOG_DEBUG << "HTTP event loop did not exit within allotted time, sending loopbreak";
                    event_base_loopbreak(event_base_ptr);
                }
                thread_http.join();
            }
            if (event_http_ptr) {
                evhttp_free(event_http_ptr);
                event_http_ptr = nullptr;
            }
            if (event_base_ptr) {
                event_base_free(event_base_ptr);
                event_base_ptr = nullptr;
            }

            // unregister_http_handler("/", false)

            LOG_DEBUG << "Stopped HTTP server";
        }

        /** Simple wrapper to set thread name and run work queue */
        void http_server_service::http_workqueue_run(work_queue<http_closure>* queue)
        {
            http_server_service::rename_thread("dbc-httpworker");
            queue->run();
        }

        void http_server_service::libevent_log_cb(int severity, const char *msg)
        {
        #ifndef EVENT_LOG_WARN
        // EVENT_LOG_WARN was added in 2.0.19; but before then _EVENT_LOG_WARN existed.
        # define EVENT_LOG_WARN _EVENT_LOG_WARN
        #endif
            if (severity >= EVENT_LOG_WARN) {  // Log warn messages and higher without debug category
                std::string  tmp_msg = "libevent: " + std::string(msg);
                LOG_INFO << tmp_msg;
            }
        }

        void http_server_service::update_http_server_logging(bool enable)
        {
        #if LIBEVENT_VERSION_NUMBER >= 0x02010100
            if (enable) {
                event_enable_debug_logging(EVENT_DBG_ALL);
            } else {
                event_enable_debug_logging(EVENT_DBG_NONE);
            }
            return;
        #else
            // Can't update libevent logging if version < 02010100
            return;
        #endif
        }

        /** HTTP request callback */
        void http_server_service::http_request_cb(struct evhttp_request* req, void* arg)
        {
            // Disable reading to work around a libevent bug, fixed in 2.2.0.
            if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
                evhttp_connection* conn = evhttp_request_get_connection(req);
                if (conn) {
                    bufferevent* bev = evhttp_connection_get_bufferevent(conn);
                    if (bev) {
                        bufferevent_disable(bev, EV_READ);
                    }
                }
            }
            assert(arg);
            work_queue<http_closure>* work_queue_tmp = reinterpret_cast<http_server_service*>(arg)->get_work_queue_ptr();
            std::unique_ptr<http_request> hreq(new http_request(req, reinterpret_cast<http_server_service*>(arg)->get_event_base_ptr()));

            LOG_INFO << "Received a " << hreq->request_method_string(hreq->get_request_method()) <<
                ", request for " << hreq->get_uri() << " from " << hreq->get_peer().get_ip();

            // todo:client allowed check,reply HTTP_FORBIDDEN

            // Early reject unknown HTTP methods
            if (hreq->get_request_method() == http_request::UNKNOWN) {
                hreq->reply_comm_rest_err(HTTP_BADMETHOD, RPC_METHOD_DEPRECATED, "not support method");
                return;
            }

            // Find registered handler for prefix
            std::string str_uri = hreq->get_uri();
            std::string path;
            std::vector<http_path_handler>::const_iterator i = reinterpret_cast<http_server_service*>(arg)->get_http_path_handler().begin();
            std::vector<http_path_handler>::const_iterator iend = reinterpret_cast<http_server_service*>(arg)->get_http_path_handler().end();
            for (; i != iend; ++i) {
                bool match = false;
                if (i->exact_match)
                    match = (str_uri == i->prefix);
                else
                    match = (str_uri.substr(0, i->prefix.size()) == i->prefix);

                if (match) {
                    path = str_uri.substr(i->prefix.size());
                    break;
                }
            }

            // Dispatch to worker thread
            if (i != iend) {
                std::unique_ptr<http_work_item> item(new http_work_item(std::move(hreq), path, i->handler));
                assert(work_queue_tmp);
                if (work_queue_tmp->enqueue(item.get()))
                    item.release(); /* if true, queue took ownership */
                else {
                    LOG_ERROR << "WARNING: request rejected because http work queue depth exceeded";
                    item->req->reply_comm_rest_err(HTTP_INTERNAL, RPC_OUT_OF_MEMORY, "Work queue depth exceeded");
                }
            } else {
                hreq->reply_comm_rest_err(HTTP_NOTFOUND, RPC_METHOD_NOT_FOUND, "not support RESTful api");
            }
        }

        void http_server_service::http_reject_request_cb(struct evhttp_request* req, void*)
        {
            LOG_INFO << "Rejecting request while shutting down";
            evhttp_send_error(req, HTTP_SERVUNAVAIL, nullptr);
        }

        bool http_server_service::thread_http_fun(struct event_base* base, struct evhttp* http)
        {
            rename_thread("dbc-http");
            LOG_DEBUG << "Entering http event loop";
            event_base_dispatch(base);
            // Event loop will be interrupted by InterruptHTTPServer()
            LOG_DEBUG << "Exited http event loop";
            return event_base_got_break(base) == 0;
        }

        bool http_server_service::http_bind_addresses(struct evhttp* http)
        {
            evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(http, rest_ip.c_str(), rest_port);
            if (bind_handle) {
                bound_sockets.push_back(bind_handle);
            } else {
            LOG_ERROR << "Binding RPC failed on address： " << rest_ip << " port: " << rest_port;
            }

            return !bound_sockets.empty();
        }

        int32_t http_server_service::load_rest_config(bpo::variables_map& options)
        {
            // rest ip addr
            std::string conf_rest_ip = options.count("rest_ip") ? options["rest_ip"].as<std::string>() : CONF_MANAGER->get_rest_ip();

            ip_validator ip_vdr;
            variable_value val;

            val.value() = conf_rest_ip;

            if (ip_vdr.validate(val) == false) {
                LOG_ERROR << "http server init invalid ip: " << conf_rest_ip;
                return E_DEFAULT;
            }
            rest_ip = conf_rest_ip;

            // rest port
            std::string conf_rest_port = options.count("rest_port") ? options["rest_port"].as<std::string>() : CONF_MANAGER->get_rest_port();

            port_validator port_vdr;

            val.value() = conf_rest_port;

            if ((conf_rest_port != "0") && (port_vdr.validate(val) == false)) {
                LOG_ERROR << "http server init invalid port: " << conf_rest_port;
                return E_DEFAULT;
            } else {
                try {
                    rest_port = (uint16_t)std::stoi(conf_rest_port);
                }
                catch (const std::exception &e) {
                    LOG_ERROR << "http server init transform exception. invalid port: " << conf_rest_port << ", " << e.what();
                    return E_DEFAULT;
                }
            }

            LOG_INFO << "rest config: " << "rest ip:" << rest_ip << ", rest port:" << rest_port;
            return E_SUCCESS;
        }

        void http_server_service::rename_thread(const char* name)
        {
        #if defined(PR_SET_NAME)
            // Only the first 15 characters are used (16 - NUL terminator)
            ::prctl(PR_SET_NAME, name, 0, 0, 0);
        #elif defined(MAC_OSX)
            pthread_setname_np(name);
        #else
            // Prevent warnings for unused parameters...
            (void)name;
        #endif
        }

        void http_server_service::register_http_handler(const std::string &prefix, bool exact_match, const http_request_handler &handler)
        {
            LOG_DEBUG << "Registering HTTP handler for: " << prefix << " exactmatch : " << exact_match;
            path_handlers.push_back(http_path_handler(prefix, exact_match, handler));
        }

        void http_server_service::unregister_http_handler(const std::string &prefix, bool exact_match)
        {
            std::vector<http_path_handler>::iterator i = path_handlers.begin();
            std::vector<http_path_handler>::iterator iend = path_handlers.end();
            for (; i != iend; ++i)
                if (i->prefix == prefix && i->exact_match == exact_match)
                    break;
            if (i != iend) {
                LOG_DEBUG << "Unregistering HTTP handler for " << prefix << " exactmatch : " << exact_match;
                path_handlers.erase(i);
            }
        }

    }  // namespace core
}  // namespace matrix
