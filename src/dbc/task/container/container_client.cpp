// container_client.cpp — see container_client.h
//
// MVP-1 status: skeleton + environment check only. Docker Engine API integration
// is TODO and tracked in DBCDEVOPS/release/PRD_container_mode_MVP1.md Week 2.

#include "container_client.h"

#include "log/log.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace dbc::container {

namespace {

// MVP-1 image whitelist: registry-prefix match. MVP-2 will read from chain.
constexpr const char* kAllowedImagePrefixes[] = {
    "nvcr.io/nvidia/pytorch",
    "nvcr.io/nvidia/cuda",
    "jupyter/tensorflow-notebook",
    "huggingface/transformers-pytorch-gpu",
    "vllm/vllm-openai",
};

// Required versions (must match PRD §8.0.2 / §8.1).
constexpr const char* kMinKataVersion = "3.4.0";
constexpr const char* kMinNvidiaToolkitVersion = "1.18.0";
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

}  // namespace

int32_t ContainerClient::CheckEnvironment() {
    namespace fs = std::filesystem;

    // 1. /dev/kvm readable+writable (Kata requires KVM).
    if (!fs::exists("/dev/kvm")) {
        LOG_ERROR << "/dev/kvm missing — Kata Containers requires KVM";
        return E_DEFAULT;
    }

    // 2. cgroups v2.
    std::string mounts;
    if (!ReadFileTrimmed("/proc/mounts", mounts) ||
        mounts.find("cgroup2 ") == std::string::npos) {
        LOG_ERROR << "cgroups v2 not mounted";
        return E_DEFAULT;
    }

    // 3. NVIDIA driver version (read /proc/driver/nvidia/version, not nvidia-smi
    //    which could be PATH-hijacked).
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

    // 4. kata-runtime version (TODO: spawn `/usr/bin/kata-runtime --version` and parse).
    // 5. nvidia-container-toolkit (TODO: read /usr/bin/nvidia-ctk version).
    // 6. Per-GPU FLR check: /sys/bus/pci/devices/<bdf>/reset_method contains 'flr' or 'bus'
    //    (TODO; enumerate via NVML).

    LOG_INFO << "container env check passed (driver " << m[1].str() << ")";
    return ERR_SUCCESS;
}

bool ContainerClient::IsImageAllowed(const std::string& image) {
    for (const char* prefix : kAllowedImagePrefixes) {
        if (image.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

int32_t ContainerClient::PullImage(const std::string& image) {
    if (!IsImageAllowed(image)) {
        LOG_ERROR << "image not in MVP-1 whitelist: " << image;
        return E_DEFAULT;
    }
    // TODO(MVP-1): POST /images/create?fromImage=... via libcurl unix-socket.
    LOG_INFO << "TODO: pull " << image;
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
    // TODO(MVP-1): POST /containers/create with:
    //   - HostConfig.Runtime = "kata-runtime"
    //   - HostConfig.PortBindings { "22/tcp": [{ HostPort: host_ssh_port }] }
    //   - HostConfig.DeviceRequests with NVIDIA driver + GPU UUID list
    //   - Env with sshd auto-start + authorized_keys
    (void)task_id; (void)host_ssh_port;
    out_container_id = "<todo>";
    return ERR_SUCCESS;
}

int32_t ContainerClient::DestroyContainer(const std::string& task_id) {
    // TODO(MVP-1): POST /containers/{id}/stop?t=30 then DELETE /containers/{id}
    (void)task_id;
    return ERR_SUCCESS;
}

int32_t ContainerClient::GetContainerStatus(const std::string& task_id,
                                            ContainerStatus& out_status) {
    // TODO(MVP-1): GET /containers/{id}/json, parse State + ExitCode + StartedAt
    (void)task_id;
    out_status = {};
    return ERR_SUCCESS;
}

int32_t ContainerClient::SetAuthorizedKeys(const std::string& task_id,
                                           const std::string& pubkey) {
    // TODO(MVP-1): build a tar stream containing /root/.ssh/authorized_keys with
    // 0600 perms, PUT /containers/{id}/archive?path=/
    (void)task_id; (void)pubkey;
    return ERR_SUCCESS;
}

}  // namespace dbc::container
