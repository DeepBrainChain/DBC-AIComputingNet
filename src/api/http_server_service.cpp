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

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/http.h>

#include "ip_validator.h"
#include "port_validator.h"
#include "server.h"
#include "log.h"

#include "rpc_error.h"
#include "http_client.h"
#include "http_server.h"
#include "http_server_service.h"

//
#include "rest_handler.h"
#include <chrono>

using namespace std::chrono;
extern std::chrono::high_resolution_clock::time_point server_start_time;

using namespace ai::dbc;

namespace matrix
{
    namespace core
    {
        int32_t http_server_service::init(bpo::variables_map& options)
        {
            int32_t ret = load_rest_config(options);
            if(ret != E_SUCCESS)
            {
                LOG_ERROR << "http server service load config error";
                return ret;
            }

            // prohibit http server if port is 0
            if(is_prohibit_rest())
            {
                return E_SUCCESS;
            }

            if(!init_http_server())
            {
                return E_EXIT_FAILURE;
            }

            return E_SUCCESS;
        }

        bool http_server_service::init_http_server()
        {
            // todo : support acl list in futurre

            raii_event_base base_ctr = obtain_event_base();

            /* Create a new evhttp object to handle requests. */
            raii_evhttp http_ctr = obtain_evhttp(base_ctr.get());
            struct evhttp *http = http_ctr.get();
            if(!http)
            {
                LOG_ERROR << "couldn't create evhttp. Exiting.";
                return false;
            }

            evhttp_set_timeout(http, DEFAULT_HTTP_SERVER_TIMEOUT);
            evhttp_set_max_headers_size(http, MAX_HEADERS_SIZE);
            evhttp_set_max_body_size(http, MAX_BODY_SIZE);
            evhttp_set_gencb(http, http_server_service::http_request_cb, this);

            if(!http_bind_addresses(http))
            {
                LOG_ERROR << "Unable to bind any endpoint for RPC server";
                return false;
            }

            m_work_queue_ptr = new work_queue<http_closure>(DEFAULT_HTTP_WORKQUEUE);
            // transfer ownership to eventBase/HTTP via .release()
            m_event_base_ptr = base_ctr.release();
            m_event_http_ptr = http_ctr.release();
            return true;
        }

        void http_server_service::start_http_server()
        {
            LOG_DEBUG << "Starting HTTP server";

            std::packaged_task<bool(event_base *, evhttp *)> task(thread_http_fun);
            m_thread_result = task.get_future();
            m_thread_http = std::thread(std::move(task), m_event_base_ptr, m_event_http_ptr);

            for(int i = 0; i < DEFAULT_HTTP_THREADS; i++)
            {
                m_thread_http_workers.emplace_back(http_server_service::http_workqueue_run, m_work_queue_ptr);
            }

            for(unsigned int i = 0; i < ARRAYLEN(uri_prefixes); i++)
            {
                register_http_handler(REST_API_URI + uri_prefixes[i].m_prefix, uri_prefixes[i].m_exact_match, uri_prefixes[i].m_handler);
            }


        }

        void http_server_service::interrupt_http_server()
        {
            LOG_INFO << "Interrupting HTTP server";
            if(m_event_http_ptr)
            {
                // Unlisten sockets
                for(evhttp_bound_socket *socket : m_bound_sockets)
                {
                    evhttp_del_accept_socket(m_event_http_ptr, socket);
                }
                // Reject requests on current connections
                evhttp_set_gencb(m_event_http_ptr, http_reject_request_cb, nullptr);
            }
            if(m_work_queue_ptr)
            {
                m_work_queue_ptr->interrupt();
            }
        }

        void http_server_service::stop_http_server()
        {
            LOG_INFO << "Stopping HTTP server";

            for(unsigned int i = 0; i < ARRAYLEN(uri_prefixes); i++)
            {
                unregister_http_handler(REST_API_URI + uri_prefixes[i].m_prefix, uri_prefixes[i].m_exact_match);
            }

            if(m_work_queue_ptr)
            {
                LOG_DEBUG << "Waiting for HTTP worker threads to exit";
                for(auto& thread : m_thread_http_workers)
                {
                    thread.join();
                }
                m_thread_http_workers.clear();
                delete m_work_queue_ptr;
                m_work_queue_ptr = nullptr;
            }
            if(m_event_base_ptr)
            {
                LOG_DEBUG << "Waiting for HTTP event thread to exit";
                // Exit the event loop as soon as there are no active events.
                event_base_loopexit(m_event_base_ptr, nullptr);
                // Give event loop a few seconds to exit (to send back last RPC responses), then break it
                // Before this was solved with event_base_loopexit, but that didn't work as expected in
                // at least libevent 2.0.21 and always introduced a delay. In libevent
                // master that appears to be solved, so in the future that solution
                // could be used again (if desirable).
                if(m_thread_result.valid() && m_thread_result.wait_for(std::chrono::milliseconds(2000)) == std::future_status::timeout)
                {
                    LOG_DEBUG << "HTTP event loop did not exit within allotted time, sending loopbreak";
                    event_base_loopbreak(m_event_base_ptr);
                }
                m_thread_http.join();
            }
            if(m_event_http_ptr)
            {
                evhttp_free(m_event_http_ptr);
                m_event_http_ptr = nullptr;
            }
            if(m_event_base_ptr)
            {
                event_base_free(m_event_base_ptr);
                m_event_base_ptr = nullptr;
            }

            LOG_DEBUG << "Stopped HTTP server";
        }

        /** Simple wrapper to set thread name and run work queue */
        void http_server_service::http_workqueue_run(work_queue<http_closure> *queue)
        {
            http_server_service::rename_thread("dbc-httpworker");
            queue->run();
        }

        /** HTTP request callback */
        void http_server_service::http_request_cb(struct evhttp_request *req, void *arg)
        {
            // Disable reading to work around a libevent bug, fixed in 2.2.0.
            if(event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001)
            {
                evhttp_connection *conn = evhttp_request_get_connection(req);
                if(conn)
                {
                    bufferevent *bev = evhttp_connection_get_bufferevent(conn);
                    if(bev)
                    {
                        bufferevent_disable(bev, EV_READ);
                    }
                }
            }
            assert(arg);
            work_queue<http_closure> *work_queue_tmp = reinterpret_cast<http_server_service *>(arg)->get_work_queue_ptr();
            std::unique_ptr<http_request> hreq(new http_request(req, reinterpret_cast<http_server_service *>(arg)->get_event_base_ptr()));

            LOG_INFO << "Received a " << hreq->request_method_string(hreq->get_request_method()) << ", request for " << hreq->get_uri() << " from " << hreq->get_peer().get_ip();

            // todo:client allowed check,reply HTTP_FORBIDDEN

            // Early reject unknown HTTP methods
            if(hreq->get_request_method() == http_request::UNKNOWN)
            {
                hreq->reply_comm_rest_err(HTTP_BADMETHOD, RPC_METHOD_DEPRECATED, "not support method");
                return;
            }


            auto time_span_ms = duration_cast<milliseconds>(high_resolution_clock::now() - server_start_time);

            if(time_span_ms.count() < MIN_INIT_HTTP_SERVER_TIME)
            {


                hreq->reply_comm_rest_err(HTTP_OK, RPC_REQUEST_INTERRUPTED, "Request is interrupted.Try again later.");

                return;
            }


            std::string str_uri = hreq->get_uri();
            if(str_uri.substr(0, REST_API_URI.size()) != REST_API_URI)
            {

                hreq->reply_comm_rest_err(HTTP_BADREQUEST, RPC_INVALID_REQUEST, "The api version is invalid. Current "
                                                                                "version is " + REST_API_URI + ".");
                return;
            }


            std::string path;
            std::vector<http_path_handler>::const_iterator i = reinterpret_cast<http_server_service *>(arg)->get_http_path_handler().begin();
            std::vector<http_path_handler>::const_iterator iend = reinterpret_cast<http_server_service *>(arg)->get_http_path_handler().end();
            for(; i != iend; ++i)
            {
                bool match = false;
                if(i->m_exact_match)
                {
                    match = (str_uri == i->m_prefix);
                }
                else
                {
                    match = (str_uri.substr(0, i->m_prefix.size()) == i->m_prefix);
                }

                if(match)
                {
                    path = str_uri.substr(i->m_prefix.size());
                    break;
                }
            }

            // Dispatch to worker thread
            if(i != iend)
            {
                std::unique_ptr<http_work_item> item(new http_work_item(std::move(hreq), path, i->m_handler));
                assert(work_queue_tmp);
                if(work_queue_tmp->enqueue(item.get()))
                {
                    item.release(); /* if true, queue took ownership */

                    LOG_INFO << "enqueue http_work_item; str_uri:" << str_uri;
                }
                else
                {
                    LOG_ERROR << "WARNING: request rejected because http work queue depth exceeded";
                    item->m_req->reply_comm_rest_err(HTTP_INTERNAL, RPC_SYSTEM_BUSYING, "Work queue depth exceeded");
                }
            }
            else
            {
                hreq->reply_comm_rest_err(HTTP_NOTFOUND, RPC_METHOD_NOT_FOUND, "not support RESTful api");
            }
        }

        void http_server_service::http_reject_request_cb(struct evhttp_request *req, void *)
        {
            LOG_INFO << "Rejecting request while shutting down";
            evhttp_send_error(req, HTTP_SERVUNAVAIL, nullptr);
        }

        bool http_server_service::thread_http_fun(struct event_base *base, struct evhttp *http)
        {
            rename_thread("dbc-http");
            LOG_DEBUG << "Entering http event loop";
            event_base_dispatch(base);
            // Event loop will be interrupted by InterruptHTTPServer()
            LOG_DEBUG << "Exited http event loop";
            return event_base_got_break(base) == 0;
        }

        bool http_server_service::http_bind_addresses(struct evhttp *http)
        {
            evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(http, m_rest_ip.c_str(), m_rest_port);
            if(bind_handle)
            {
                m_bound_sockets.push_back(bind_handle);
            }
            else
            {
                LOG_ERROR << "Binding RPC failed on address： " << m_rest_ip << " port: " << m_rest_port;
            }

            return !m_bound_sockets.empty();
        }

        int32_t http_server_service::load_rest_config(bpo::variables_map& options)
        {
            // rest ip addr
            std::string conf_rest_ip = options.count("rest_ip")?options["rest_ip"].as<std::string>():CONF_MANAGER->get_rest_ip();

            ip_validator ip_vdr;
            variable_value val;

            val.value() = conf_rest_ip;

            if(ip_vdr.validate(val) == false)
            {
                LOG_ERROR << "http server init invalid ip: " << conf_rest_ip;
                return E_DEFAULT;
            }
            m_rest_ip = conf_rest_ip;

            // rest port
            std::string conf_rest_port = options.count("rest_port")?options["rest_port"].as<std::string>():CONF_MANAGER->get_rest_port();

            port_validator port_vdr;

            val.value() = conf_rest_port;

            if((conf_rest_port != "0") && (port_vdr.validate(val) == false))
            {
                LOG_ERROR << "http server init invalid port: " << conf_rest_port;
                return E_DEFAULT;
            }
            else
            {
                try
                {
                    m_rest_port = (uint16_t) std::stoi(conf_rest_port);
                }
                catch(const std::exception& e)
                {
                    LOG_ERROR << "http server init transform exception. invalid port: " << conf_rest_port << ", " << e.what();
                    return E_DEFAULT;
                }
            }

            LOG_INFO << "rest config: " << "rest ip:" << m_rest_ip << ", rest port:" << m_rest_port;
            return E_SUCCESS;
        }

        void http_server_service::rename_thread(const char *name)
        {
#if defined(PR_SET_NAME)
            // Only the first 15 characters are used (16 - NUL terminator)
            ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif defined(MAC_OSX)
            pthread_setname_np(name);
#else
            // Prevent warnings for unused parameters...
            (void) name;
#endif
        }

        void http_server_service::register_http_handler(const std::string& prefix, bool exact_match, const http_request_handler& handler)
        {
            LOG_DEBUG << "Registering HTTP handler for: " << prefix << " exactmatch : " << exact_match;
            m_path_handlers.push_back(http_path_handler(prefix, exact_match, handler));
        }

        void http_server_service::unregister_http_handler(const std::string& prefix, bool exact_match)
        {
            std::vector<http_path_handler>::iterator i = m_path_handlers.begin();
            std::vector<http_path_handler>::iterator iend = m_path_handlers.end();
            for(; i != iend; ++i)
                if(i->m_prefix == prefix && i->m_exact_match == exact_match)
                {
                    break;
                }
            if(i != iend)
            {
                LOG_DEBUG << "Unregistering HTTP handler for " << prefix << " exactmatch : " << exact_match;
                m_path_handlers.erase(i);
            }
        }

    }  // namespace core
}  // namespace matrix
