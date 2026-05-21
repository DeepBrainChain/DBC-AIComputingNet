// sidecar_rpc_client.h
//
// JSON-RPC client to talk to the local `dbc-chain-sidecar` (Rust binary,
// separate repo: DeepBrainChain/dbc-chain-sidecar). Sidecar owns the substrate
// connection + sr25519 signing; dbc-node delegates all chain work to it.
//
// Transport: Unix socket (/var/run/dbc-node/sidecar.sock).

#ifndef DBC_SIDECAR_RPC_CLIENT_H
#define DBC_SIDECAR_RPC_CLIENT_H

#include "util/utils.h"

#include <cstdint>
#include <optional>
#include <string>

namespace dbc::sidecar {

struct TxOutcome {
    std::string tx_hash;
    std::string block_hash;
    uint32_t block_number = 0;
};

struct ContainerInfoView {
    uint32_t bonded_at_block = 0;
    bool spec_proof_committee_verified = false;
    uint16_t port_range_start = 0;
    uint16_t port_range_end = 0;
};

class SidecarRpcClient : public Singleton<SidecarRpcClient> {
public:
    /// Set the Unix socket path. Connection is lazy (first call dials).
    int32_t Init(const std::string& socket_path);

    /// Submit `containerMode.enable_container_mode` and wait for finalization.
    int32_t EnableContainerMode(
        const std::string& machine_id_hex,
        uint16_t port_range_start,
        uint16_t port_range_end,
        TxOutcome& out_outcome);

    /// Submit `containerMode.remove_container_mode`.
    int32_t RemoveContainerMode(
        const std::string& machine_id_hex,
        TxOutcome& out_outcome);

    /// Read `containerMode.containerModeMachines(machine_id)` storage.
    /// Returns ERR_SUCCESS + std::nullopt if the machine is not in container mode.
    int32_t QueryContainerInfo(
        const std::string& machine_id_hex,
        std::optional<ContainerInfoView>& out_info);

    /// Healthcheck.
    bool IsAlive();

private:
    friend class Singleton<SidecarRpcClient>;
    SidecarRpcClient() = default;

    std::string socket_path_;

    /// Helper: open a fresh AF_UNIX socket, send JSON-RPC request, read response,
    /// return parsed JSON. Atomic per-call.
    /// MVP-1 uses synchronous I/O; can be made async later.
    int32_t CallSync(
        const std::string& method,
        const std::string& params_json,
        std::string& out_result_json);
};

}  // namespace dbc::sidecar

#endif  // DBC_SIDECAR_RPC_CLIENT_H
