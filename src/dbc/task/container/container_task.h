// container_task.h
//
// Bridge between TaskManager (which speaks CreateTaskParams) and the lower-
// level ContainerClient + PortAllocator + DdnWsClient. Keeps the existing
// VM-mode flow untouched: TaskManager dispatches by params.task_type and only
// container-mode tasks reach here.

#ifndef DBC_CONTAINER_TASK_H
#define DBC_CONTAINER_TASK_H

#include "container_client.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dbc::container {

/// Minimal subset of CreateTaskParams we need; populated by TaskManager when
/// it routes a container-mode rental here.
struct ContainerTaskRequest {
    std::string task_id;                     // shared with TaskInfo / rent_id
    std::string image;                       // must be in MVP-1 whitelist
    std::vector<std::string> gpu_uuids;      // NVIDIA UUIDs, integer cards only
    int64_t mem_size_kb = 0;
    int64_t cpu_quota_us = 0;
    std::string ssh_authorized_keys;         // injected via SetAuthorizedKeys
};

struct ContainerTaskResult {
    std::string container_id;
    uint16_t host_ssh_port = 0;              // NAT'd to container :22
};

/// Container task lifecycle. Each call is idempotent on failure: anything that
/// allocates a port + creates a container must clean up if any later step
/// fails. The TaskManager-side TaskInfo persistence is the source of truth
/// for "is this rental alive"; this module just stitches the docker call +
/// port reservation + chain notification together.
class ContainerTask {
public:
    /// Create + start the container. On success, port is reserved in
    /// PortAllocator and the SSH key is injected. On any failure the partial
    /// state is rolled back (port released, container removed).
    static int32_t Start(const ContainerTaskRequest& req, ContainerTaskResult& out);

    /// Stop + remove the container; release the host port back to the pool.
    static int32_t Stop(const std::string& task_id, uint16_t allocated_port);
};

}  // namespace dbc::container

#endif  // DBC_CONTAINER_TASK_H
