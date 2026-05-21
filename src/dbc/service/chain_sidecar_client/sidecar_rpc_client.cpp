// sidecar_rpc_client.cpp — see sidecar_rpc_client.h
//
// MVP-1: skeleton. Real Unix-socket JSON-RPC client to be wired up alongside
// the Rust sidecar (DeepBrainChain/dbc-chain-sidecar).

#include "sidecar_rpc_client.h"

#include "log/log.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace dbc::sidecar {

int32_t SidecarRpcClient::Init(const std::string& socket_path) {
    socket_path_ = socket_path;
    if (!IsAlive()) {
        LOG_WARNING << "sidecar not reachable at " << socket_path
                    << " (will retry on first call)";
    }
    return ERR_SUCCESS;
}

bool SidecarRpcClient::IsAlive() {
    if (socket_path_.empty()) return false;
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());
    bool ok = (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    ::close(fd);
    return ok;
}

int32_t SidecarRpcClient::CallSync(
    const std::string& method,
    const std::string& params_json,
    std::string& out_result_json) {

    // TODO(MVP-1):
    // 1. Open AF_UNIX SOCK_STREAM to socket_path_.
    // 2. Send: {"jsonrpc":"2.0","id":1,"method":method,"params":params_json}\n
    // 3. Read line-delimited JSON response.
    // 4. Parse + extract "result" or "error".
    (void)method; (void)params_json; (void)out_result_json;
    LOG_DEBUG << "TODO: sidecar CallSync(" << method << ")";
    return ERR_SUCCESS;
}

int32_t SidecarRpcClient::EnableContainerMode(
    const std::string& machine_id_hex,
    uint16_t port_range_start,
    uint16_t port_range_end,
    TxOutcome& out_outcome) {

    char params[256];
    std::snprintf(params, sizeof(params),
        R"({"machine_id_hex":"%s","port_range_start":%u,"port_range_end":%u})",
        machine_id_hex.c_str(),
        static_cast<unsigned>(port_range_start),
        static_cast<unsigned>(port_range_end));

    std::string result;
    int32_t rc = CallSync("enable_container_mode", params, result);
    if (rc != ERR_SUCCESS) return rc;

    // TODO(MVP-1): JSON-parse result into out_outcome.
    (void)out_outcome;
    return ERR_SUCCESS;
}

int32_t SidecarRpcClient::RemoveContainerMode(
    const std::string& machine_id_hex,
    TxOutcome& out_outcome) {

    char params[128];
    std::snprintf(params, sizeof(params),
        R"({"machine_id_hex":"%s"})", machine_id_hex.c_str());

    std::string result;
    int32_t rc = CallSync("remove_container_mode", params, result);
    if (rc != ERR_SUCCESS) return rc;
    (void)out_outcome;
    return ERR_SUCCESS;
}

int32_t SidecarRpcClient::QueryContainerInfo(
    const std::string& machine_id_hex,
    std::optional<ContainerInfoView>& out_info) {

    char params[128];
    std::snprintf(params, sizeof(params),
        R"({"machine_id_hex":"%s"})", machine_id_hex.c_str());

    std::string result;
    int32_t rc = CallSync("query_container_info", params, result);
    if (rc != ERR_SUCCESS) return rc;

    // TODO(MVP-1): if result is `null` → out_info = std::nullopt; else parse fields.
    (void)out_info;
    return ERR_SUCCESS;
}

}  // namespace dbc::sidecar
