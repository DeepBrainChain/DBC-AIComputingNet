// port_allocator.h
//
// Host-port pool for the renter-facing SSH/HTTPS NAT into Kata containers.
// MVP-1: in-memory set + sqlite-persisted state file for restart recovery.

#ifndef DBC_PORT_ALLOCATOR_H
#define DBC_PORT_ALLOCATOR_H

#include "util/utils.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace dbc::container {

class PortAllocator : public Singleton<PortAllocator> {
public:
    /// Initialize with the host port range. start < end, end - start >= 8.
    /// Re-init resets state; call once at server boot.
    int32_t Configure(uint16_t start, uint16_t end);

    /// Allocate `count` contiguous ports (best-effort). Returns empty vec on
    /// failure. Currently MVP-1 only needs 1-2 ports per rental (sshd + Jupyter),
    /// no contiguity requirement.
    std::vector<uint16_t> Allocate(uint8_t count);

    /// Return ports to the pool.
    void Release(const std::vector<uint16_t>& ports);

    /// Reserve specific ports (e.g., recovered from existing rentals on startup).
    int32_t Reserve(const std::vector<uint16_t>& ports);

    /// Persist allocator state to disk. Called on every allocate/release.
    int32_t SaveState(const std::string& path);

    /// Restore allocator state. Called once at boot before Allocate.
    int32_t LoadState(const std::string& path);

    /// Number of ports still free.
    size_t FreeCount() const;

private:
    friend class Singleton<PortAllocator>;
    PortAllocator() = default;

    mutable std::mutex mtx_;
    uint16_t range_start_ = 30000;
    uint16_t range_end_ = 40000;
    std::set<uint16_t> free_ports_;
    std::set<uint16_t> allocated_ports_;
    std::string state_path_;
};

}  // namespace dbc::container

#endif  // DBC_PORT_ALLOCATOR_H
