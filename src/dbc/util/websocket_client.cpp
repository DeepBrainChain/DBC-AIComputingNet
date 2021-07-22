#include "websocket_client.h"

void websocket_client::init(const std::string& url) {
    ix::initNetSystem();
    m_web_socket.setUrl(url);
    m_web_socket.setOnMessageCallback([this] (const ix::WebSocketMessagePtr& msg) {
           if (msg->type == ix::WebSocketMessageType::Message) {
               if (m_msg_callback) {
                   m_msg_callback(0, msg->str);
               }
           }
           else if (msg->type == ix::WebSocketMessageType::Open) {
               m_connected = true;
           }
           else if (msg->type == ix::WebSocketMessageType::Error) {
               if (m_msg_callback) {
                   m_msg_callback(-1, msg->errorInfo.reason);
               }
           }
    });
}

void websocket_client::set_message_callback(const std::function<void(int32_t err_code, const std::string& msg)> &callback) {
    m_msg_callback = callback;
}

void websocket_client::start() {
    m_web_socket.start();
}

void websocket_client::stop() {
    m_web_socket.stop();
    sleep(3);
    ix::uninitNetSystem();
}

void websocket_client::send(const std::string &message) {
    m_web_socket.send(message);
}

bool websocket_client::is_connected() {
    ix::ReadyState stat = m_web_socket.getReadyState();
    return stat == ix::ReadyState::Open;
}

void websocket_client::enable_auto_reconnection() {
    m_web_socket.enableAutomaticReconnection();
}