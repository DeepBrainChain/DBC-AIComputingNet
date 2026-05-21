// ddn_ws_client.h
//
// WebSocket long connection to a DDN (DistributedDetectionNode, Go service)
// for liveness reporting in container mode. DDN times out at 90s + 60s grace,
// then issues `report_machine_soft_offline` on chain.

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

/// Wire-compatible with DistributedDetectionNode's existing wsHeader + body
/// format. Project tag is "DBC-AI-Container" for container-mode machines.
struct HeartbeatMsg {
    std::string machine_id;        // hex-encoded substrate MachineId
    uint64_t timestamp_ms;
    std::string project = "DBC-AI-Container";
};

class DdnWsClient : public Singleton<DdnWsClient> {
public:
    /// Connect to wss://ddn.dbcwallet.io/ws (configurable).
    int32_t Connect(const std::string& ws_url, const std::string& machine_id);

    /// Drop the connection. Heartbeat thread is joined.
    void Disconnect();

    /// Current connection state (for /api/v1/status reporting).
    ConnState State() const { return state_.load(); }

    /// Push container state transitions (Running, Exited, Dead) to DDN out of band.
    /// Default heartbeat already carries machine_id alive; this is for granular
    /// per-rental health.
    int32_t NotifyContainerState(const std::string& task_id, int container_state);

    /// Callback invoked when the WS connection drops; container task manager
    /// should pause new rentals.
    void OnDisconnect(std::function<void()> cb) { on_disconnect_ = std::move(cb); }

private:
    friend class Singleton<DdnWsClient>;
    DdnWsClient() = default;
    ~DdnWsClient();

    void HeartbeatLoop();

    std::atomic<ConnState> state_{ConnState::Disconnected};
    std::string ws_url_;
    std::string machine_id_;
    std::thread heartbeat_thread_;
    std::atomic<bool> shutdown_{false};
    std::function<void()> on_disconnect_;

    // TODO(MVP-1): websocketpp client handle with TLS context.
};

}  // namespace dbc::ddn

#endif  // DBC_DDN_WS_CLIENT_H
