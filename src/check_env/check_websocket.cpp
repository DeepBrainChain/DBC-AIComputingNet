#include "check_websocket.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"

/*
namespace check_websocket {
    std::mutex g_mutex_check_ws;
    std::condition_variable g_cond_check_ws;

    void test_websocket(const std::string& ws_url, const std::string& task_id) {
        // Required on Windows
        ix::initNetSystem();

        // Our websocket object
        ix::WebSocket webSocket;

        // Connect to a server with encryption
        // See https://machinezone.github.io/IXWebSocket/usage/#tls-support-and-configuration
        webSocket.setUrl(ws_url);

        std::cout << ix::userAgent() << std::endl;
        //std::cout << "Connecting to " << url << std::endl;

        // Setup a callback to be fired (in a background thread, watch out for race conditions !)
        // when a message or an event (open, close, error) is received
        webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Message)
            {
                rapidjson::Document doc;
                doc.Parse(msg->str.c_str());
                if (!doc.IsObject()) {
                    std::cout << "websocket parse recevied message error" << std::endl;
                } else {
                    std::cout << "received message: len=" << msg->str.size() << std::endl;
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Open)
            {
                std::cout << "Connection established" << std::endl;
            }
            else if (msg->type == ix::WebSocketMessageType::Error)
            {
                // Maybe SSL is not configured properly
                std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
            }

            g_cond_check_ws.notify_one();
        }
        );

        // Now that our callback is setup, we can start our background thread and receive messages
        webSocket.start();

        sleep(1);

        // Send a message to the server (default to TEXT mode)
        while (true) {
            if (webSocket.getReadyState() == ix::ReadyState::Open) {
                std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
                        + task_id + R"("]})";
                webSocket.send(str_send);
                break;
            } else {
                sleep(1);
            }
        };

        std::unique_lock<std::mutex> lock(g_mutex_check_ws);
        g_cond_check_ws.wait(lock);
        webSocket.stop();
        sleep(1);
        ix::uninitNetSystem();
    }
}
*/