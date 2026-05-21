// container_task.cpp — see container_task.h

#include "container_task.h"

#include "container_client.h"
#include "log/log.h"
#include "port_allocator.h"
#include "util/utils.h"

namespace dbc::container {

int32_t ContainerTask::Start(const ContainerTaskRequest& req,
                             ContainerTaskResult& out) {
    out = {};

    // 1. Reserve a host port for the renter's SSH ingress.
    auto ports = PortAllocator::GetInstance().Allocate(/*count=*/1);
    if (ports.empty()) {
        LOG_ERROR << "container task " << req.task_id
                  << ": port pool exhausted";
        return E_DEFAULT;
    }
    const uint16_t host_ssh_port = ports[0];

    // 2. Pull image (idempotent — Docker engine returns 200 even if cached).
    if (ContainerClient::GetInstance().PullImage(req.image) != ERR_SUCCESS) {
        PortAllocator::GetInstance().Release(ports);
        return E_DEFAULT;
    }

    // 3. Create + start the Kata container with GPU passthrough.
    ContainerSpec spec;
    spec.image = req.image;
    spec.gpu_uuids = req.gpu_uuids;
    spec.mem_size_kb = req.mem_size_kb;
    spec.cpu_quota_us = req.cpu_quota_us;
    spec.ssh_authorized_keys = req.ssh_authorized_keys;

    std::string container_id;
    if (ContainerClient::GetInstance().CreateContainer(
            req.task_id, host_ssh_port, spec, container_id) != ERR_SUCCESS) {
        PortAllocator::GetInstance().Release(ports);
        return E_DEFAULT;
    }

    // 4. Inject SSH pubkey. On failure we tear down the container so we don't
    //    leave the renter with no way in.
    if (!req.ssh_authorized_keys.empty()) {
        if (ContainerClient::GetInstance().SetAuthorizedKeys(
                req.task_id, req.ssh_authorized_keys) != ERR_SUCCESS) {
            LOG_ERROR << "container task " << req.task_id
                      << ": SetAuthorizedKeys failed; rolling back";
            ContainerClient::GetInstance().DestroyContainer(req.task_id);
            PortAllocator::GetInstance().Release(ports);
            return E_DEFAULT;
        }
    }

    out.container_id = container_id;
    out.host_ssh_port = host_ssh_port;
    LOG_INFO << "container task " << req.task_id << " ready: container="
             << container_id.substr(0, 12) << " ssh=" << host_ssh_port;
    return ERR_SUCCESS;
}

int32_t ContainerTask::Stop(const std::string& task_id, uint16_t allocated_port) {
    int32_t rc = ContainerClient::GetInstance().DestroyContainer(task_id);
    // Release the port regardless of destroy result; we don't want orphaned
    // reservations on the host.
    if (allocated_port != 0) {
        PortAllocator::GetInstance().Release({ allocated_port });
    }
    return rc;
}

}  // namespace dbc::container
