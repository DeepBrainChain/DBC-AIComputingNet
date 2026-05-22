// sidecar_rpc_client.cpp — see sidecar_rpc_client.h
//
// AF_UNIX SOCK_STREAM, line-delimited JSON-RPC 2.0 over /var/run/dbc-node/sidecar.sock.
// Sidecar enforces SO_PEERCRED on the other end; we just need to talk plain
// JSON.

#include "sidecar_rpc_client.h"

#include "log/log.h"

#include "rapidjson/document.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace dbc::sidecar {

namespace {

constexpr int kRpcTimeoutSec = 90;
std::atomic<uint64_t> g_rpc_id{0};
std::mutex g_call_mtx;  // serialise calls; sidecar is single-shot per connection in MVP-1

/// Write `data` (exactly len bytes) to fd, retrying on EINTR/short writes.
[[nodiscard]] bool WriteAll(int fd, const char* data, size_t len) {
    while (len > 0) {
        ssize_t n = ::send(fd, data, len, MSG_NOSIGNAL);
        if (n > 0) { data += n; len -= n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

/// Read until '\n' or peer-close. Returns the line (without trailing '\n')
/// or empty on error.
[[nodiscard]] bool ReadLine(int fd, std::string& out, int timeout_sec) {
    out.clear();
    char buf[4096];
    timeval tv{};
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            out.append(buf, n);
            auto pos = out.find('\n');
            if (pos != std::string::npos) {
                out.resize(pos);
                return true;
            }
            continue;
        }
        if (n == 0) return !out.empty();  // peer closed mid-line
        if (errno == EINTR) continue;
        LOG_ERROR << "sidecar recv failed: " << std::strerror(errno);
        return false;
    }
}

}  // namespace

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

    if (socket_path_.empty()) {
        LOG_ERROR << "sidecar not initialised";
        return E_DEFAULT;
    }

    std::lock_guard<std::mutex> lk(g_call_mtx);

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR << "sidecar socket() failed: " << std::strerror(errno);
        return E_DEFAULT;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERROR << "sidecar connect(" << socket_path_ << ") failed: " << std::strerror(errno);
        ::close(fd);
        return E_DEFAULT;
    }

    timeval tv{};
    tv.tv_sec = kRpcTimeoutSec;
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    uint64_t id = g_rpc_id.fetch_add(1) + 1;
    char envelope[8192];
    int len = std::snprintf(envelope, sizeof(envelope),
        R"({"jsonrpc":"2.0","id":%llu,"method":"%s","params":%s})" "\n",
        static_cast<unsigned long long>(id), method.c_str(),
        params_json.empty() ? "{}" : params_json.c_str());
    if (len <= 0 || static_cast<size_t>(len) >= sizeof(envelope)) {
        LOG_ERROR << "sidecar request too large";
        ::close(fd);
        return E_DEFAULT;
    }

    if (!WriteAll(fd, envelope, len)) {
        LOG_ERROR << "sidecar write failed: " << std::strerror(errno);
        ::close(fd);
        return E_DEFAULT;
    }

    std::string line;
    bool ok = ReadLine(fd, line, kRpcTimeoutSec);
    ::close(fd);
    if (!ok || line.empty()) {
        LOG_ERROR << "sidecar empty/timeout response (method=" << method << ")";
        return E_DEFAULT;
    }

    rapidjson::Document d;
    if (d.Parse(line.c_str()).HasParseError() || !d.IsObject()) {
        LOG_ERROR << "sidecar bad JSON: " << line.substr(0, 200);
        return E_DEFAULT;
    }
    if (d.HasMember("error") && !d["error"].IsNull()) {
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> w(sb);
        d["error"].Accept(w);
        LOG_ERROR << "sidecar RPC error (" << method << "): " << sb.GetString();
        return E_DEFAULT;
    }
    if (!d.HasMember("result")) {
        LOG_ERROR << "sidecar missing result for " << method;
        return E_DEFAULT;
    }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    d["result"].Accept(w);
    out_result_json.assign(sb.GetString(), sb.GetSize());
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

    rapidjson::Document d;
    if (d.Parse(result.c_str()).HasParseError() || !d.IsObject()) {
        LOG_ERROR << "bad TxOutcome JSON: " << result.substr(0, 200);
        return E_DEFAULT;
    }
    out_outcome.tx_hash = d.HasMember("tx_hash") ? d["tx_hash"].GetString() : "";
    out_outcome.block_hash = d.HasMember("block_hash") ? d["block_hash"].GetString() : "";
    out_outcome.block_number = d.HasMember("block_number") ? d["block_number"].GetUint() : 0;
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

    rapidjson::Document d;
    if (d.Parse(result.c_str()).HasParseError() || !d.IsObject()) return E_DEFAULT;
    out_outcome.tx_hash = d.HasMember("tx_hash") ? d["tx_hash"].GetString() : "";
    out_outcome.block_hash = d.HasMember("block_hash") ? d["block_hash"].GetString() : "";
    out_outcome.block_number = d.HasMember("block_number") ? d["block_number"].GetUint() : 0;
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

    rapidjson::Document d;
    if (d.Parse(result.c_str()).HasParseError()) return E_DEFAULT;
    if (d.IsNull()) {
        out_info = std::nullopt;
        return ERR_SUCCESS;
    }
    if (!d.IsObject()) return E_DEFAULT;

    ContainerInfoView v;
    v.bonded_at_block =
        d.HasMember("bonded_at_block") ? d["bonded_at_block"].GetUint() : 0;
    v.spec_proof_committee_verified =
        d.HasMember("spec_proof_committee_verified") &&
        d["spec_proof_committee_verified"].GetBool();
    v.port_range_start =
        d.HasMember("port_range_start")
            ? static_cast<uint16_t>(d["port_range_start"].GetUint())
            : 0;
    v.port_range_end =
        d.HasMember("port_range_end")
            ? static_cast<uint16_t>(d["port_range_end"].GetUint())
            : 0;
    out_info = v;
    return ERR_SUCCESS;
}

}  // namespace dbc::sidecar
