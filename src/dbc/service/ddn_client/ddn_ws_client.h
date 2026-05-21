// ddn_ws_client.h
//
// WebSocket long connection to a DDN (DistributedDetectionNode, Go service)
// for liveness reporting in container mode. DDN times out at 90s + 60s grace,
// then issues `report_machine_soft_offline` on chain.
//
// Implementation lives in ddn_ws_client.cpp and uses websocketpp (header-only)
// over Boost.Asio TLS. The pimpl `Impl` keeps the WS types out of this header
// so consumers don't pull in websocketpp transitively.

#ifndef DBC_DDN_WS_CLIENT_H
#define DBC_DDN_WS_CLIENT_H

#include "util/utils.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace dbc::ddn {

enum class ConnState {
    Disconnected,
    Connecting,
    Connected,
    Failed,
};

class DdnWsClient : public Singleton<DdnWsClient> {
public:
    /// Connect to wss://ddn.dbcwallet.io/ws (configurable). Returns immediately;
    /// real handshake runs in a background asio thread. Use State() to poll.
    int32_t Connect(const std::string& ws_url, const std::string& machine_id);

    /// Drop the connection, stop the heartbeat thread, join the asio thread.
    void Disconnect();

    /// Current connection state (cheap atomic read).
    ConnState State() const { return state_.load(); }

    /// Push a container-state transition (Running, Exited, Dead) to the DDN.
    /// Best-effort — drops the message silently if not connected.
    int32_t NotifyContainerState(const std::string& task_id, int container_state);

    /// Callback invoked when the WS connection drops; the container task
    /// manager should pause new rental matching until reconnected.
    void OnDisconnect(std::function<void()> cb) { on_disconnect_ = std::move(cb); }

private:
    friend class Singleton<DdnWsClient>;
    DdnWsClient() = default;
    ~DdnWsClient();

    void HeartbeatLoop();
    void SendRegistration();
    int32_t SendFrame(const std::string& payload);

    std::atomic<ConnState> state_{ConnState::Disconnected};
    std::string ws_url_;
    std::string machine_id_;
    std::thread heartbeat_thread_;
    std::atomic<bool> shutdown_{false};
    std::function<void()> on_disconnect_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dbc::ddn

#endif  // DBC_DDN_WS_CLIENT_H
