// container_client.cpp — see container_client.h
//
// MVP-1 status:
//   * CheckEnvironment / IsImageAllowed — real implementations
//   * PullImage / CreateContainer / DestroyContainer / GetContainerStatus —
//     real implementations against Docker Engine API over its unix socket
//   * SetAuthorizedKeys — stub pending tar-stream builder (Week 2 mid-sprint)

#include "container_client.h"

#include "docker_http_client.h"
#include "log/log.h"

#include "3rd/network/rapidjson/document.h"
#include "3rd/network/rapidjson/stringbuffer.h"
#include "3rd/network/rapidjson/writer.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace dbc::container {

namespace {

// MVP-1 image whitelist: registry-prefix match. MVP-2 moves to on-chain list.
constexpr const char* kAllowedImagePrefixes[] = {
    "nvcr.io/nvidia/pytorch",
    "nvcr.io/nvidia/cuda",
    "jupyter/tensorflow-notebook",
    "huggingface/transformers-pytorch-gpu",
    "vllm/vllm-openai",
};

constexpr const char* kMinNvidiaDriverVersion = "560.35.05";

[[nodiscard]] bool ReadFileTrimmed(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    out = ss.str();
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return true;
}

[[nodiscard]] bool VersionAtLeast(const std::string& have, const std::string& need) {
    auto parse = [](const std::string& v) {
        std::vector<int> parts;
        std::stringstream ss(v);
        std::string seg;
        while (std::getline(ss, seg, '.')) {
            try { parts.push_back(std::stoi(seg)); } catch (...) { parts.push_back(0); }
        }
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    };
    auto h = parse(have), n = parse(need);
    for (size_t i = 0; i < std::min(h.size(), n.size()); ++i) {
        if (h[i] > n[i]) return true;
        if (h[i] < n[i]) return false;
    }
    return true;
}

DockerHttpClient& docker() {
    static DockerHttpClient c;
    return c;
}

}  // namespace

// ----- environment check + whitelist (unchanged) -----

int32_t ContainerClient::CheckEnvironment() {
    namespace fs = std::filesystem;

    if (!fs::exists("/dev/kvm")) {
        LOG_ERROR << "/dev/kvm missing — Kata Containers requires KVM";
        return E_DEFAULT;
    }

    std::string mounts;
    if (!ReadFileTrimmed("/proc/mounts", mounts) ||
        mounts.find("cgroup2 ") == std::string::npos) {
        LOG_ERROR << "cgroups v2 not mounted";
        return E_DEFAULT;
    }

    std::string drv;
    if (!ReadFileTrimmed("/proc/driver/nvidia/version", drv)) {
        LOG_ERROR << "NVIDIA driver not loaded";
        return E_DEFAULT;
    }
    std::smatch m;
    static const std::regex kDriverRe(R"(Kernel Module\s+([\d\.]+))");
    if (!std::regex_search(drv, m, kDriverRe) ||
        !VersionAtLeast(m[1].str(), kMinNvidiaDriverVersion)) {
        LOG_ERROR << "NVIDIA driver " << (m.size() > 1 ? m[1].str() : "?")
                  << " < required " << kMinNvidiaDriverVersion;
        return E_DEFAULT;
    }

    // Smoke-test the docker socket while we're here.
    HttpResponse r;
    if (docker().Get("/_ping", r) != ERR_SUCCESS || r.status_code != 200) {
        LOG_ERROR << "docker engine /_ping failed (code=" << r.status_code << ")";
        return E_DEFAULT;
    }

    LOG_INFO << "container env check passed (driver " << m[1].str() << ")";
    return ERR_SUCCESS;
}

bool ContainerClient::IsImageAllowed(const std::string& image) {
    for (const char* prefix : kAllowedImagePrefixes) {
        if (image.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

// ----- Docker Engine API integrations -----

int32_t ContainerClient::PullImage(const std::string& image) {
    if (!IsImageAllowed(image)) {
        LOG_ERROR << "image not in MVP-1 whitelist: " << image;
        return E_DEFAULT;
    }
    HttpResponse r;
    // Docker engine streams progress; we don't care about body content.
    std::string path = "/images/create?fromImage=" + image;
    int32_t rc = docker().PostJson(path, "{}", r);
    if (rc != ERR_SUCCESS) return rc;
    if (r.status_code != 200) {
        LOG_ERROR << "pull image failed: HTTP " << r.status_code << " body=" << r.body.substr(0, 200);
        return E_DEFAULT;
    }
    LOG_INFO << "pulled " << image;
    return ERR_SUCCESS;
}

int32_t ContainerClient::CreateContainer(
    const std::string& task_id,
    uint16_t host_ssh_port,
    const ContainerSpec& spec,
    std::string& out_container_id) {

    if (!IsImageAllowed(spec.image)) {
        LOG_ERROR << "image not whitelisted: " << spec.image;
        return E_DEFAULT;
    }
    if (spec.gpu_uuids.empty()) {
        LOG_ERROR << "container task " << task_id << " requested 0 GPUs";
        return E_DEFAULT;
    }

    using namespace rapidjson;
    Document d; d.SetObject();
    auto& a = d.GetAllocator();

    d.AddMember("Image", Value(spec.image.c_str(), a), a);

    // ExposedPorts: { "22/tcp": {} }
    Value exposed(kObjectType);
    exposed.AddMember("22/tcp", Value(kObjectType), a);
    d.AddMember("ExposedPorts", exposed, a);

    // HostConfig
    Value host(kObjectType);
    host.AddMember("Runtime", Value("kata-runtime", a), a);
    host.AddMember("AutoRemove", true, a);

    // PortBindings: { "22/tcp": [{ "HostPort": "30231" }] }
    Value portBindings(kObjectType);
    Value binding(kArrayType);
    Value entry(kObjectType);
    entry.AddMember("HostPort", Value(std::to_string(host_ssh_port).c_str(), a), a);
    binding.PushBack(entry, a);
    portBindings.AddMember("22/tcp", binding, a);
    host.AddMember("PortBindings", portBindings, a);

    // DeviceRequests for NVIDIA GPUs:
    //   [{ Driver: "nvidia", Count: -1, DeviceIDs: [uuid1, uuid2], Capabilities: [["gpu","compute","utility"]] }]
    Value devReq(kObjectType);
    devReq.AddMember("Driver", Value("nvidia", a), a);
    Value ids(kArrayType);
    for (const auto& u : spec.gpu_uuids) ids.PushBack(Value(u.c_str(), a), a);
    devReq.AddMember("DeviceIDs", ids, a);
    Value caps(kArrayType);
    Value capRow(kArrayType);
    capRow.PushBack(Value("gpu", a), a);
    capRow.PushBack(Value("compute", a), a);
    capRow.PushBack(Value("utility", a), a);
    caps.PushBack(capRow, a);
    devReq.AddMember("Capabilities", caps, a);
    Value devReqs(kArrayType); devReqs.PushBack(devReq, a);
    host.AddMember("DeviceRequests", devReqs, a);

    // Optional cgroup caps
    if (spec.mem_size_kb > 0) host.AddMember("Memory", spec.mem_size_kb * 1024, a);
    if (spec.cpu_quota_us > 0) host.AddMember("CpuQuota", spec.cpu_quota_us, a);

    d.AddMember("HostConfig", host, a);

    // Cmd: keep image default; sshd-start is the image's responsibility for
    // MVP-1 (NGC pytorch images already ship sshd).

    StringBuffer sb;
    Writer<StringBuffer> w(sb);
    d.Accept(w);

    HttpResponse resp;
    const std::string path = "/containers/create?name=" + task_id;
    int32_t rc = docker().PostJson(path, sb.GetString(), resp);
    if (rc != ERR_SUCCESS) return rc;
    if (resp.status_code != 201) {
        LOG_ERROR << "container create failed: HTTP " << resp.status_code
                  << " body=" << resp.body.substr(0, 300);
        return E_DEFAULT;
    }

    // Parse { "Id": "...", "Warnings": [...] }
    Document r2;
    if (r2.Parse(resp.body.c_str()).HasParseError() || !r2.HasMember("Id")) {
        LOG_ERROR << "container create: bad response " << resp.body.substr(0, 200);
        return E_DEFAULT;
    }
    out_container_id = r2["Id"].GetString();

    // Start the container.
    HttpResponse startResp;
    rc = docker().PostJson("/containers/" + out_container_id + "/start", "", startResp);
    if (rc != ERR_SUCCESS) return rc;
    if (startResp.status_code != 204) {
        LOG_ERROR << "container start failed: HTTP " << startResp.status_code
                  << " body=" << startResp.body.substr(0, 300);
        // Best-effort cleanup so we don't leave a half-started container.
        HttpResponse delResp;
        docker().Delete("/containers/" + out_container_id + "?force=true", delResp);
        return E_DEFAULT;
    }

    LOG_INFO << "created+started container " << out_container_id.substr(0, 12)
             << " task=" << task_id << " ssh=" << host_ssh_port;
    return ERR_SUCCESS;
}

int32_t ContainerClient::DestroyContainer(const std::string& task_id) {
    HttpResponse stopResp;
    // 30s graceful stop; engine SIGKILLs after timeout.
    int32_t rc = docker().PostJson("/containers/" + task_id + "/stop?t=30", "", stopResp);
    // 304 = already stopped; treat as success
    if (rc != ERR_SUCCESS) return rc;
    if (stopResp.status_code != 204 && stopResp.status_code != 304 &&
        stopResp.status_code != 404) {
        LOG_WARNING << "container stop returned HTTP " << stopResp.status_code
                    << " body=" << stopResp.body.substr(0, 200);
    }

    HttpResponse delResp;
    // force=true ensures removal even if still running for whatever reason.
    rc = docker().Delete("/containers/" + task_id + "?force=true&v=true", delResp);
    if (rc != ERR_SUCCESS) return rc;
    if (delResp.status_code != 204 && delResp.status_code != 404) {
        LOG_ERROR << "container delete failed: HTTP " << delResp.status_code
                  << " body=" << delResp.body.substr(0, 200);
        return E_DEFAULT;
    }

    LOG_INFO << "destroyed container task=" << task_id;
    return ERR_SUCCESS;
}

int32_t ContainerClient::GetContainerStatus(const std::string& task_id,
                                            ContainerStatus& out_status) {
    HttpResponse r;
    int32_t rc = docker().Get("/containers/" + task_id + "/json", r);
    if (rc != ERR_SUCCESS) return rc;
    if (r.status_code == 404) {
        out_status = {};
        out_status.state = ContainerState::Unknown;
        return ERR_SUCCESS;
    }
    if (r.status_code != 200) {
        LOG_ERROR << "container inspect HTTP " << r.status_code;
        return E_DEFAULT;
    }
    rapidjson::Document d;
    if (d.Parse(r.body.c_str()).HasParseError() || !d.HasMember("State")) {
        LOG_ERROR << "container inspect: bad json";
        return E_DEFAULT;
    }
    out_status.container_id = d.HasMember("Id") ? d["Id"].GetString() : "";
    const auto& s = d["State"];
    std::string status = s.HasMember("Status") ? s["Status"].GetString() : "unknown";
    if (status == "created") out_status.state = ContainerState::Created;
    else if (status == "running") out_status.state = ContainerState::Running;
    else if (status == "paused") out_status.state = ContainerState::Paused;
    else if (status == "exited") out_status.state = ContainerState::Exited;
    else if (status == "dead") out_status.state = ContainerState::Dead;
    else out_status.state = ContainerState::Unknown;
    out_status.exit_code = s.HasMember("ExitCode") ? s["ExitCode"].GetInt() : 0;
    return ERR_SUCCESS;
}

int32_t ContainerClient::SetAuthorizedKeys(const std::string& task_id,
                                           const std::string& pubkey) {
    // TODO(MVP-1 Week 2): build a tar stream containing /root/.ssh/authorized_keys
    // with 0600 perms, send via PUT /containers/{id}/archive?path=/.
    // Stub until then — for MVP-1 the rental image is responsible for accepting
    // SSH_AUTHORIZED_KEYS as an env var and writing it itself.
    (void)task_id; (void)pubkey;
    return ERR_SUCCESS;
}

}  // namespace dbc::container
