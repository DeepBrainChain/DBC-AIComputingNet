#ifndef DBC_NETWORK_HTTP_CLIENT_H
#define DBC_NETWORK_HTTP_CLIENT_H

#include <utility>
#include "common/common.h"
#include <prettywriter.h>
#include <event2/event.h>
#include <event2/http.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/rand.h>
#include <event2/bufferevent_ssl.h>

#define DEFAULT_HTTP_TIME_OUT                 30

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
////////////////////////////
//MAKE_RAII(bufferevent);
MAKE_RAII(evhttp_uri);

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

inline raii_evhttp_connection obtain_evhttp_connection_base2(struct event_base* base, struct bufferevent *bev, std::string host, uint16_t port)
{
    auto result = raii_evhttp_connection(evhttp_connection_base_bufferevent_new(base, nullptr, bev, host.c_str(), port));
    if (!result.get())
        throw std::runtime_error("create connection failed");
    return result;
}

inline raii_evhttp_uri obtain_evhttp_uri_parse(const std::string & url)
{
    auto result = raii_evhttp_uri(evhttp_uri_parse(url.c_str()));
    if (!result.get())
        throw std::runtime_error("parse url error");
    return result;
}

//struct evhttp_uri *http_uri = evhttp_uri_parse(url.c_str());

inline bufferevent * obtain_evhttp_bev(struct event_base* base, SSL *ssl)
{
    bufferevent * bev = nullptr;
    if (ssl != nullptr)
    {
        bev = bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
        if (!bev)
        {
            return nullptr;
        }
            
        bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);
        return bev;
    }
    else
    {
        bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
        if (!bev)
        {
            return nullptr;
        }
        return bev;
    }
    return bev;
}

namespace network
{
    struct http_response
    {
        http_response() : status(0), error(-1) {}

        int status;
        int error;          //evhttp_request_error
        std::string body;
    };

    struct http_response_wrapper
    {
        http_response* http_resp;
        event_base* ev_base;
    };

    class http_client
    {
    public:

        http_client(std::string remote_ip, uint16_t remote_port);

        virtual ~http_client();
        http_client(const std::string &url, const std::string & crt);

        void set_remote(std::string remote_ip, uint16_t remote_port);

        int32_t post(const std::string &endpoint, const kvs &headers, const std::string & req_content, http_response &resp);
        int32_t post_sleep(const std::string &endpoint, const kvs &headers, const std::string & req_content, http_response &resp,int32_t sleep_time);

        int32_t get(const std::string &endpoint, const kvs &headers, http_response &resp);
        int32_t get_sleep(const std::string &endpoint, const kvs &headers, http_response &resp,int32_t sleep_time);
        int32_t del(const std::string &endpoint, const kvs &headers, http_response &resp);

        int32_t parse_url(const std::string & url);

        std::string get_uri() { return m_uri; }
        std::string  get_remote_host();

        void set_address(std::string remote_ip, uint16_t remote_port);

    protected:
        bool init_ssl_ctx();
        bool clear_ssl_ctx();
        bool start_ssl_engine();
        bool stop_ssl_engine();

    protected:
        std::string m_remote_ip;

        uint16_t m_remote_port;

        std::string m_uri="";
        std::string m_scheme="";
        SSL_CTX* m_ssl_ctx = nullptr;
        SSL* m_ssl = nullptr;
        std::string m_crt;
    };
}

#endif
