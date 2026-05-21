// ddn_ws_client.cpp — see ddn_ws_client.h
//
// MVP-1: skeleton with state machine + thread plumbing; actual websocketpp wire
// up is TODO. Compiles standalone without a WS library.

#include "ddn_ws_client.h"

#include "log/log.h"

#include <chrono>

namespace dbc::ddn {

namespace {
constexpr int kHeartbeatIntervalSec = 20;
}

DdnWsClient::~DdnWsClient() {
    Disconnect();
}

int32_t DdnWsClient::Connect(const std::string& ws_url, const std::string& machine_id) {
    if (state_.load() != ConnState::Disconnected) {
        LOG_WARNING << "DdnWsClient already connected/connecting";
        return E_DEFAULT;
    }
    ws_url_ = ws_url;
    machine_id_ = machine_id;
    shutdown_.store(false);
    state_.store(ConnState::Connecting);

    // TODO(MVP-1): construct websocketpp::client<asio_tls_client>, set up
    // on_open / on_message / on_close handlers, start asio thread.

    // Start heartbeat thread — fires every 20s when state == Connected.
    heartbeat_thread_ = std::thread([this] { HeartbeatLoop(); });
    LOG_INFO << "DdnWsClient connecting to " << ws_url << " for machine " << machine_id;
    return ERR_SUCCESS;
}

void DdnWsClient::Disconnect() {
    shutdown_.store(true);
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    state_.store(ConnState::Disconnected);
    // TODO(MVP-1): close ws client + join asio thread.
    LOG_INFO << "DdnWsClient disconnected";
}

int32_t DdnWsClient::NotifyContainerState(const std::string& task_id, int container_state) {
    if (state_.load() != ConnState::Connected) {
        LOG_WARNING << "NotifyContainerState while not connected; dropped";
        return E_DEFAULT;
    }
    // TODO(MVP-1): build NotifyMsg JSON and send via websocketpp.
    LOG_DEBUG << "TODO: ws send NotifyContainerState " << task_id << "=" << container_state;
    return ERR_SUCCESS;
}

void DdnWsClient::HeartbeatLoop() {
    while (!shutdown_.load()) {
        if (state_.load() == ConnState::Connected) {
            // TODO(MVP-1): build HeartbeatMsg JSON; ws send.
            LOG_DEBUG << "TODO: ws send heartbeat machine_id=" << machine_id_;
        }
        for (int i = 0; i < kHeartbeatIntervalSec * 10 && !shutdown_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

}  // namespace dbc::ddn
