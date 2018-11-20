/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : http_server.cpp
* description       : http server request process
* date              : 2018.11.9
* author            : tower
**********************************************************************************/

#include <assert.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/http.h>

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "container_message.h"

#include "log.h"
#include "rpc_error.h"
#include "http_server.h"

namespace matrix
{
    namespace core
    {
        http_request::http_request(struct evhttp_request* req_, struct event_base* event_base_) : req(req_),
            reply_sent(false), event_base_ptr(event_base_)
        {
        }

        http_request::~http_request()
        {
            if (!reply_sent) {
                // Keep track of whether reply was sent to avoid request leaks
                LOG_DEBUG <<  "Unhandled request";
                reply_comm_rest_err(HTTP_INTERNAL, RPC_INTERNAL_ERROR, "Unhandled request");
            }
            // evhttpd cleans up the request, as long as a reply was sent.
        }

        std::string http_request::get_uri()
        {
            return evhttp_request_get_uri(req);
        }

        endpoint_address http_request::get_peer()
        {
            evhttp_connection* con = evhttp_request_get_connection(req);

            const char* address = "";
            uint16_t port = 0;
            if (con) {
                // evhttp retains ownership over returned address string
                evhttp_connection_get_peer(con, (char**)&address, &port);
            }
            return endpoint_address(address, port);
        }

        http_request::REQUEST_METHOD http_request::get_request_method()
        {
            switch (evhttp_request_get_command(req)) {
                case EVHTTP_REQ_GET:
                    return GET;
                    break;
                case EVHTTP_REQ_POST:
                    return POST;
                    break;
                case EVHTTP_REQ_HEAD:
                    return HEAD;
                    break;
                case EVHTTP_REQ_PUT:
                    return PUT;
                    break;
                default:
                    return UNKNOWN;
                    break;
            }
        }

        std::pair<bool, std::string> http_request::get_header(const std::string& hdr)
        {
            const struct evkeyvalq* headers = evhttp_request_get_input_headers(req);
            assert(headers);
            const char* val = evhttp_find_header(headers, hdr.c_str());
            if (val)
                return std::make_pair(true, val);
            else
                return std::make_pair(false, "");
        }

        std::string http_request::read_body()
        {
            struct evbuffer* buf = evhttp_request_get_input_buffer(req);
            if (!buf)
                return "";
            size_t size = evbuffer_get_length(buf);
            /** Trivial implementation: if this is ever a performance bottleneck,
             * internal copying can be avoided in multi-segment buffers by using
             * evbuffer_peek and an awkward loop. Though in that case, it'd be even
             * better to not copy into an intermediate string but use a stream
             * abstraction to consume the evbuffer on the fly in the parsing algorithm.
             */
            const char* data = (const char*)evbuffer_pullup(buf, size);
            if (!data)  // returns nullptr in case of empty buffer
                return "";
            std::string rv(data, size);
            evbuffer_drain(buf, size);
           return rv;
        }

        void http_request::write_header(const std::string& hdr, const std::string& value)
        {
            struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
            assert(headers);
            evhttp_add_header(headers, hdr.c_str(), value.c_str());
        }

        /** Closure sent to main thread to request a reply to be sent to
         * a HTTP request.
         * Replies must be sent in the main loop in the main http thread,
         * this cannot be done from worker threads.
         */
        void http_request::write_reply(int status, const std::string& reply)
        {
            assert(!reply_sent && req);
            // Send event to main http thread twrite_replyo send reply message
            struct evbuffer* evb = evhttp_request_get_output_buffer(req);
            assert(evb);
            evbuffer_add(evb, reply.data(), reply.size());
            auto req_copy = req;
            //  http_event release in http event callback function
            http_event* ev = new http_event(event_base_ptr, true, [req_copy, status]{
                evhttp_send_reply(req_copy, status, nullptr, nullptr);
                // Re-enable reading from the socket. This is the second part of the libevent
                // workaround above.
                if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
                    evhttp_connection* conn = evhttp_request_get_connection(req_copy);
                    if (conn) {
                        bufferevent* bev = evhttp_connection_get_bufferevent(conn);
                        if (bev) {
                            bufferevent_enable(bev, EV_READ | EV_WRITE);
                        }
                    }
                }
            });
            ev->trigger(nullptr);
            reply_sent = true;
            req = nullptr; // transferred back to main thread
        }

        // response http request comm err
        // status:genernal http status code, internal_error: dbc define error code
        // message:response error content
        void http_request::reply_comm_rest_err(uint32_t status, int32_t internal_error, std::string message)
        {
            //  construct body
            rapidjson::Document document;
            rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

            rapidjson::Value root(rapidjson::kObjectType);
            root.AddMember("error_code", internal_error, allocator);
            root.AddMember("error_message", STRING_REF(message), allocator);

            std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
            root.Accept(writer);

            write_header("Content-Type", "application/json");
            write_reply(status, std::string(buffer->GetString()) + "\r\n");
        }

        std::string http_request::request_method_string(REQUEST_METHOD method)
        {
            switch (method) {
                case http_request::GET:
                    return "GET";
                    break;
                case http_request::POST:
                    return "POST";
                    break;
                case http_request::HEAD:
                    return "HEAD";
                    break;
                case http_request::PUT:
                    return "PUT";
                    break;
                default:
                    return "unknown";
            }
        }

        http_event::http_event(struct event_base* base, bool delete_when_triggered_, const std::function<void(void)>& handler_):
            delete_when_triggered(delete_when_triggered_), handler(handler_)
        {
            ev = event_new(base, -1, 0, http_event::httpevent_callback_fn, this);
            assert(ev);
        }

        http_event::~http_event()
        {
            event_free(ev);
        }

        void http_event::trigger(struct timeval* tv)
        {
            if (tv == nullptr)
                event_active(ev, 0, 0);  // immediately trigger event in main thread
            else
                evtimer_add(ev, tv);  // trigger after timeval passed
        }

        void http_event::httpevent_callback_fn(evutil_socket_t /**/, short /**/, void* data)
        {
            // Static handler: simply call inner handler
            http_event *self = static_cast<http_event*>(data);
            self->handler();
            if (self->delete_when_triggered)
                delete self;
        }

    }  // namespace core
}  // namespace matrix
