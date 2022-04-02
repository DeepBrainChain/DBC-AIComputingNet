#ifndef DBC_HTTP_SERVER_SERVICE_H
#define DBC_HTTP_SERVER_SERVICE_H

#include "util/utils.h"
#include "network/http/http_server.h"

#define DEFAULT_HTTP_SERVER_TIMEOUT     30
#define MAX_HEADERS_SIZE     8192
#define MAX_BODY_SIZE    0x02000000

class http_server_service : public Singleton<http_server_service> {
public:
    http_server_service() = default;

    virtual ~http_server_service() = default;

    ERRCODE init();

    void exit();

private:
    ERRCODE load_config();

    bool init_http_server();

    bool http_bind_addresses(struct evhttp *http);

    static void http_request_cb(struct evhttp_request *req, void *arg);

    void on_http_request_event(struct evhttp_request *req);

    void start_http_server();

    static bool thread_http_fun(struct event_base *base, struct evhttp *http);

    static void rename_thread(const char *name);

    void stop_http_server();

    void interrupt_http_server();

    static void http_reject_request_cb(struct evhttp_request *req, void *);

private:
    std::string m_listen_ip = "127.0.0.1";
    uint16_t m_listen_port = 0;

    struct event_base *m_event_base = nullptr;
    struct evhttp *m_event_http = nullptr;
    std::vector<evhttp_bound_socket *> m_bound_sockets;

    std::thread m_thread_http;
    std::future<bool> m_thread_result;
};

#endif
