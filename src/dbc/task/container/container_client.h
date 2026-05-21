// container_client.h
//
// Kata Containers + nvidia-container-toolkit integration for dbc-node's
// container-mode TaskManager branch (MVP-1). Talks to the local Docker Engine
// API over libcurl; does NOT fork-exec `docker` CLI.
//
// See DBCDEVOPS/release/DBC_NODE_CONTAINER_MODE_DESIGN.md §3 for design.

#ifndef DBC_CONTAINER_CLIENT_H
#define DBC_CONTAINER_CLIENT_H

#include "util/utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dbc::container {

/// Per-rental container spec (subset of CreateTaskParams that matters for Kata).
struct ContainerSpec {
    std::string image;                   // must be in MVP-1 whitelist
    std::vector<std::string> gpu_uuids;  // NVIDIA GPU UUIDs to passthrough
    int64_t mem_size_kb = 0;             // optional cgroup memory cap (0 = unlimited)
    int64_t cpu_quota_us = 0;            // optional cpu.cfs_quota_us (0 = unlimited)
    std::string ssh_authorized_keys;     // user pubkey injected into /root/.ssh/
};

enum class ContainerState {
    Unknown,
    Created,
    Running,
    Paused,
    Exited,
    Dead,
};

struct ContainerStatus {
    ContainerState state = ContainerState::Unknown;
    std::string container_id;       // docker engine container id (64 hex)
    int exit_code = 0;
    uint64_t started_at_ms = 0;
};

class ContainerClient : public Singleton<ContainerClient> {
public:
    /// One-shot environment check; called from Server::Init before listening.
    /// Verifies kata-runtime ≥3.4.0, nvidia-container-toolkit ≥1.18.0, driver
    /// ≥560.35.05, cgroups v2, /dev/kvm readable, GPU FLR reset_method.
    static int32_t CheckEnvironment();

    /// Pull image if missing. Image MUST be in the MVP-1 whitelist
    /// (nvcr.io/nvidia/pytorch:24.05-py3, etc.). Returns ERR_SUCCESS on success.
    int32_t PullImage(const std::string& image);

    /// Create + start a Kata-runtime container. Port `host_ssh_port` on the host
    /// is NAT'd to container :22. Returns ERR_SUCCESS + populates out_container_id.
    int32_t CreateContainer(
        const std::string& task_id,
        uint16_t host_ssh_port,
        const ContainerSpec& spec,
        std::string& out_container_id);

    /// Stop + remove the container (graceful 30s, then force).
    int32_t DestroyContainer(const std::string& task_id);

    /// Inspect container; populates out_status.
    int32_t GetContainerStatus(const std::string& task_id, ContainerStatus& out_status);

    /// Inject (or replace) authorized_keys for the container's root user.
    /// MVP-1: tar-stream into /root/.ssh/authorized_keys via docker cp API.
    int32_t SetAuthorizedKeys(const std::string& task_id, const std::string& pubkey);

    /// MVP-1 whitelist check (registry prefix). MVP-2 will move to on-chain list.
    static bool IsImageAllowed(const std::string& image);

private:
    friend class Singleton<ContainerClient>;
    ContainerClient() = default;

    // libcurl HTTP client against /var/run/docker.sock (Unix socket).
    // TODO(MVP-1): implement DockerEngine wrapper (POST /containers/create,
    // /containers/{id}/start, /containers/{id}/json, /containers/{id}/stop).
    // Filter HostConfig.Runtime = "kata-runtime"; DeviceRequests for GPUs.
};

}  // namespace dbc::container

#endif  // DBC_CONTAINER_CLIENT_H
