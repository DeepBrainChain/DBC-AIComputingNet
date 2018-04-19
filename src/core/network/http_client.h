/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºhttp_client.h
* description    £ºhttp client for rpc
* date                  : 2018.04.07
* author            £ºBruce Feng
**********************************************************************************/

#pragma once


#include <utility>
#include "common.h"
#include <stringbuffer.h>
#include <document.h>
#include <prettywriter.h>
#include <event2/event.h>
#include <event2/http.h>


#define DEFAULT_HTTP_TIME_OUT                 3

using kvs = typename std::list<std::pair<std::string, std::string>>;


#define MAKE_RAII(type) \
        struct type##_deleter {\
            void operator()(struct type* ob) {\
                type##_free(ob);\
            }\
        };\
        typedef std::unique_ptr<struct type, type##_deleter> raii_##type

MAKE_RAII(event_base);
MAKE_RAII(event);
MAKE_RAII(evhttp);
MAKE_RAII(evhttp_request);
MAKE_RAII(evhttp_connection);

inline raii_event_base obtain_event_base() 
{
    auto result = raii_event_base(event_base_new());
    if (!result.get())
        throw std::runtime_error("cannot create event_base");
    return result;
}

inline raii_event obtain_event(struct event_base* base, evutil_socket_t s, short events, event_callback_fn cb, void* arg) 
{
    return raii_event(event_new(base, s, events, cb, arg));
}

inline raii_evhttp obtain_evhttp(struct event_base* base) 
{
    return raii_evhttp(evhttp_new(base));
}

inline raii_evhttp_request obtain_evhttp_request(void(*cb)(struct evhttp_request *, void *), void *arg) 
{
    return raii_evhttp_request(evhttp_request_new(cb, arg));
}

inline raii_evhttp_connection obtain_evhttp_connection_base(struct event_base* base, std::string host, uint16_t port) 
{
    auto result = raii_evhttp_connection(evhttp_connection_base_new(base, nullptr, host.c_str(), port));
    if (!result.get())
        throw std::runtime_error("create connection failed");
    return result;
}


namespace matrix
{
    namespace core
    {

        struct http_response
        {
            http_response() : status(0), error(-1) {}

            int status;
            int error;          //evhttp_request_error
            std::string body;
        };

        class http_client
        {
        public:

            http_client(std::string remote_ip, uint16_t remote_port);

            virtual ~http_client() = default;

            void set_remote(std::string remote_ip, uint16_t remote_port);

            int32_t post(const std::string &endpoint, const kvs &headers, const std::string & req_content, http_response &resp);

            int32_t get(const std::string &endpoint, const kvs &headers, http_response &resp);

            int32_t del(const std::string &endpoint, const kvs &headers, http_response &resp);

        protected:

            std::string m_remote_ip;

            uint16_t m_remote_port;

        };

    }

}