#ifndef DBC_NETWORK_HTTP_SERVER_H
#define DBC_NETWORK_HTTP_SERVER_H

#include <event2/util.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/http.h>
#include "network/utils/endpoint_address.h"
#include "util/utils.h"
#include "../protocol/net_message.h"

namespace network
{
    class http_request
    {
        private:
        struct evhttp_request* m_req;
        bool m_reply_sent;
        struct event_base* m_event_base_ptr;

        public:
        explicit http_request(struct evhttp_request* req_, struct event_base* event_base_);
        ~http_request();

        enum REQUEST_METHOD
        {
            UNKNOWN,
            GET,
            POST,
            HEAD,
            PUT
        };

        /** Get requested URI.
            */
        std::string get_uri();

        /** Get endpoint_address(address:ip) for the origin of the http request.
            */
        endpoint_address get_peer();

        /** Get request method.
            */
        REQUEST_METHOD get_request_method();

        /**
            * Get the request header specified by hdr, or an empty string.
            * Return a pair (isPresent,string).
            */
        std::pair<bool, std::string> get_header(const std::string& hdr);

        /**
            * Read request body.
            *
            * @note As this consumes the underlying buffer, call this only once.
            * Repeated calls will return an empty string.
            */
        std::string read_body();

        /**
            * Write output header.
            *
            * @note call this before calling WriteErrorReply or Reply.
            */
        void write_header(const std::string& hdr, const std::string& value);

        /**
            * Write HTTP reply.
            * nStatus is the HTTP status code to send.
            * strReply is the body of the reply. Keep it empty to send a standard message.
            *
            * @note Can be called only once. As this will give the request back to the
            * main thread, do not call any other HTTPRequest methods after calling this.
            */
        void write_reply(int status, const std::string& reply = "");

        /** HTTP request method as string - use for logging only */
        std::string request_method_string(REQUEST_METHOD method);

        void reply_comm_rest_err(uint32_t status, int32_t internal_error, std::string message);

        void reply_comm_rest_succ( rapidjson::Value &data);

        void reply_comm_rest_err2(uint32_t status, const std::string& json);

        void reply_comm_rest_succ2(const std::string& json);
    };

    /** Event class. This can be used either as a cross-thread trigger or as a timer.
        */
    class http_event
    {
        public:
        /** Create a new event.
            * deleteWhenTriggered deletes this event object after the event is triggered (and the handler called)
            * handler is the handler to call when the event is triggered.
            */
        http_event(struct event_base* base, bool delete_when_triggered_, const std::function<void(void)>& handler_);
        ~http_event();

        /** Trigger the event. If tv is 0, trigger it immediately. Otherwise trigger it after
        * the given time has elapsed.
        */
        void trigger(struct timeval* tv);

        static void httpevent_callback_fn(evutil_socket_t /**/, short /**/, void* data);

        public:
        bool m_delete_when_triggered;
        std::function<void(void)> m_handler;

        private:
        struct event* m_ev;
    };

    //Record requests for a channel for easy storage in session
    struct http_request_context
    {
        std::shared_ptr<http_request> m_hreq;
        std::shared_ptr<message> m_req_msg;
        std::string peer_node_id;

        http_request_context()
        {
        }
    };

    typedef std::shared_ptr<http_request>  HTTP_REQUEST_PTR;
    typedef std::shared_ptr<http_request_context> HTTP_REQ_CTX_PTR;

    typedef std::function<void(const HTTP_REQUEST_PTR&, const std::string& )> http_request_handler;
    typedef std::function<void(const HTTP_REQ_CTX_PTR&, const std::shared_ptr<message>& )> response_call_handler;

    struct http_path_handler
    {
        http_path_handler() {}
        http_path_handler(std::string prefix_, bool exact_match_, http_request_handler handler_):
            m_prefix(prefix_), m_exact_match(exact_match_), m_handler(handler_)
        {
        }
        std::string m_prefix;
        bool m_exact_match;
        http_request_handler m_handler;
    };

    struct response_msg_handler
    {
        response_msg_handler()
        {}

        response_msg_handler(std::string name_, response_call_handler handler_) :
            name(name_), handler(handler_)
        {
        }

        std::string name;
        response_call_handler handler;
    };

    class http_request_event
    {
        public:
        virtual void on_http_request_event(std::shared_ptr<http_request>& hreq)=0;
    };
}  // namespce core

#endif
