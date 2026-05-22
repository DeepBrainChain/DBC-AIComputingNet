// ddn_ws_client.cpp — see ddn_ws_client.h
//
// websocketpp-based client with TLS over Boost.Asio. websocketpp is header-only
// (Apache 2.0), pulled in via `#include <websocketpp/...>` so no extra link
// step in CMakeLists beyond -lssl -lcrypto -lboost_system (already there).
//
// Wire format mirrors DistributedDetectionNode's existing format: each frame
// is a single JSON object with a `wsHeader` envelope. We only emit two kinds
// of frames in MVP-1:
//   * `online` registration (sent on connect)
//   * `heartbeat`           (every 20s)
//
// Reconnection: on close/fail we sleep 5s and try again, indefinitely.
// Caller (Server::Init) is expected to set OnDisconnect() to surface the
// status to the rest of dbc-node.

#include "ddn_ws_client.h"

#include "log/log.h"
#include "util/utils.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "rapidjson/document.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>

namespace dbc::ddn {

namespace {

using WsClient = websocketpp::client<websocketpp::config::asio_tls_client>;
using ContextPtr = std::shared_ptr<boost::asio::ssl::context>;

constexpr int kHeartbeatIntervalSec = 20;
constexpr int kReconnectBackoffSec = 5;

ContextPtr OnTlsInit() {
    auto ctx = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tlsv12_client);
    try {
        ctx->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);
        // MVP-1: trust the system CA bundle (Let's Encrypt cert on
        // ddn.dbcwallet.io). MVP-2 will pin a specific cert.
        ctx->set_default_verify_paths();
        ctx->set_verify_mode(boost::asio::ssl::verify_peer);
    } catch (const std::exception& e) {
        LOG_ERROR << "DDN tls init: " << e.what();
    }
    return ctx;
}

}  // namespace

// Opaque inner state held via std::unique_ptr to keep the header free of
// websocketpp #includes. Saves recompiling everything that pulls in
// ddn_ws_client.h when the WS library version moves.
struct DdnWsClient::Impl {
    WsClient client;
    WsClient::connection_ptr conn;
    std::mutex conn_mtx;
    std::thread asio_thread;
};

DdnWsClient::~DdnWsClient() {
    Disconnect();
}

int32_t DdnWsClient::Connect(const std::string& ws_url, const std::string& machine_id) {
    if (state_.load() != ConnState::Disconnected) {
        LOG_WARNING << "DdnWsClient already connected/connecting";
        return E_DEFAULT;
    }
    if (!impl_) impl_ = std::make_unique<Impl>();
    ws_url_ = ws_url;
    machine_id_ = machine_id;
    shutdown_.store(false);
    state_.store(ConnState::Connecting);

    try {
        impl_->client.clear_access_channels(websocketpp::log::alevel::all);
        impl_->client.clear_error_channels(websocketpp::log::elevel::all);
        impl_->client.init_asio();
        impl_->client.start_perpetual();
        impl_->client.set_tls_init_handler([](websocketpp::connection_hdl) { return OnTlsInit(); });
        impl_->client.set_open_handler([this](websocketpp::connection_hdl) {
            state_.store(ConnState::Connected);
            LOG_INFO << "DdnWsClient connected to " << ws_url_;
            SendRegistration();
        });
        impl_->client.set_close_handler([this](websocketpp::connection_hdl) {
            state_.store(ConnState::Disconnected);
            LOG_WARNING << "DdnWsClient connection closed; will retry";
            if (on_disconnect_) on_disconnect_();
        });
        impl_->client.set_fail_handler([this](websocketpp::connection_hdl) {
            state_.store(ConnState::Failed);
            LOG_WARNING << "DdnWsClient handshake failed; will retry";
            if (on_disconnect_) on_disconnect_();
        });
        impl_->client.set_message_handler(
            [this](websocketpp::connection_hdl, WsClient::message_ptr msg) {
                LOG_DEBUG << "DdnWsClient recv: " << msg->get_payload().substr(0, 200);
                // MVP-1: we don't act on incoming messages, only liveness matters.
                // MVP-2 will parse DeepLink-style commands here.
            });

        websocketpp::lib::error_code ec;
        auto conn = impl_->client.get_connection(ws_url_, ec);
        if (ec) {
            LOG_ERROR << "DdnWsClient get_connection: " << ec.message();
            state_.store(ConnState::Failed);
            return E_DEFAULT;
        }
        {
            std::lock_guard<std::mutex> lk(impl_->conn_mtx);
            impl_->conn = conn;
        }
        impl_->client.connect(conn);

        impl_->asio_thread = std::thread([this] {
            try { impl_->client.run(); }
            catch (const std::exception& e) {
                LOG_ERROR << "DdnWsClient asio thread exited: " << e.what();
            }
        });
    } catch (const std::exception& e) {
        LOG_ERROR << "DdnWsClient Connect: " << e.what();
        state_.store(ConnState::Failed);
        return E_DEFAULT;
    }

    heartbeat_thread_ = std::thread([this] { HeartbeatLoop(); });
    return ERR_SUCCESS;
}

void DdnWsClient::Disconnect() {
    shutdown_.store(true);
    if (impl_) {
        try {
            impl_->client.stop_perpetual();
            websocketpp::lib::error_code ec;
            std::lock_guard<std::mutex> lk(impl_->conn_mtx);
            if (impl_->conn && impl_->conn->get_state() ==
                websocketpp::session::state::open) {
                impl_->client.close(impl_->conn, websocketpp::close::status::going_away, "bye", ec);
            }
        } catch (...) {}
    }
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (impl_ && impl_->asio_thread.joinable()) impl_->asio_thread.join();
    state_.store(ConnState::Disconnected);
    LOG_INFO << "DdnWsClient disconnected";
}

int32_t DdnWsClient::NotifyContainerState(const std::string& task_id, int container_state) {
    if (state_.load() != ConnState::Connected) {
        LOG_WARNING << "NotifyContainerState while not connected; dropped";
        return E_DEFAULT;
    }
    char buf[512];
    int len = std::snprintf(buf, sizeof(buf),
        R"({"type":"notify","machine_id":"%s","task_id":"%s","state":%d,"ts":%lld})",
        machine_id_.c_str(), task_id.c_str(), container_state,
        static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    return SendFrame(std::string(buf, len));
}

void DdnWsClient::HeartbeatLoop() {
    while (!shutdown_.load()) {
        if (state_.load() == ConnState::Connected) {
            char buf[256];
            int len = std::snprintf(buf, sizeof(buf),
                R"({"type":"heartbeat","machine_id":"%s","project":"DBC-AI-Container","ts":%lld})",
                machine_id_.c_str(),
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()));
            (void)SendFrame(std::string(buf, len));
        } else if (state_.load() == ConnState::Failed && !shutdown_.load()) {
            // Reconnect attempt.
            std::this_thread::sleep_for(std::chrono::seconds(kReconnectBackoffSec));
            state_.store(ConnState::Connecting);
            try {
                websocketpp::lib::error_code ec;
                auto conn = impl_->client.get_connection(ws_url_, ec);
                if (!ec) {
                    {
                        std::lock_guard<std::mutex> lk(impl_->conn_mtx);
                        impl_->conn = conn;
                    }
                    impl_->client.connect(conn);
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "DdnWsClient reconnect: " << e.what();
            }
        }
        for (int i = 0; i < kHeartbeatIntervalSec * 10 && !shutdown_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

int32_t DdnWsClient::SendFrame(const std::string& payload) {
    std::lock_guard<std::mutex> lk(impl_->conn_mtx);
    if (!impl_->conn) return E_DEFAULT;
    try {
        websocketpp::lib::error_code ec;
        impl_->client.send(impl_->conn, payload, websocketpp::frame::opcode::text, ec);
        if (ec) {
            LOG_WARNING << "DdnWsClient send: " << ec.message();
            return E_DEFAULT;
        }
    } catch (const std::exception& e) {
        LOG_WARNING << "DdnWsClient send threw: " << e.what();
        return E_DEFAULT;
    }
    return ERR_SUCCESS;
}

void DdnWsClient::SendRegistration() {
    char buf[256];
    int len = std::snprintf(buf, sizeof(buf),
        R"({"type":"online","machine_id":"%s","project":"DBC-AI-Container","staking_type":3})",
        machine_id_.c_str());
    (void)SendFrame(std::string(buf, len));
}

}  // namespace dbc::ddn
