#ifndef DBCPROJ_WEBSOCKET_CLIENT_H
#define DBCPROJ_WEBSOCKET_CLIENT_H

#include <iostream>
#include "ixwebsocket/IXNetSystem.h"
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXUserAgent.h"

class websocket_client {
public:
    void init(const std::string& url);

    void set_message_callback(const std::function<void(int32_t err_code, const std::string& msg)>& callback);

    void start();

    void stop();

    void send(const std::string& message);

    bool is_connected();

    void enable_auto_reconnection();

private:
    ix::WebSocket m_web_socket;
    std::function<void(int32_t err_code, const std::string& msg)> m_msg_callback;
    bool m_connected = false;
    std::condition_variable m_cond;
};

#endif //DBCPROJ_WEBSOCKET_CLIENT_H
