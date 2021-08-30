#ifndef DBCPROJ_SYSTEM_INFO_H
#define DBCPROJ_SYSTEM_INFO_H

#include <iostream>
#include <string>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <cstring>
#include "singleton.h"

enum OS_TYPE {
    OS_1804,
    OS_2004
};

struct mem_info {
    uint64_t mem_total = 0L;
    uint64_t mem_free = 0L;
    uint64_t mem_used = 0L;
    uint64_t mem_available = 0L;
    uint64_t mem_buffers = 0L;
    uint64_t mem_cached = 0L;
    uint64_t mem_swap_total = 0L;
    uint64_t mem_swap_free = 0L;
    uint64_t mem_shared = 0L;
    float mem_usage = 0.0f;
};

struct cpu_info {
    std::string cpu_name;
    int32_t physical_cores = 0;
    int32_t physical_cores_per_cpu = 0;
    int32_t logical_cores_per_cpu = 0;
    int32_t threads_per_cpu = 0;
    int32_t total_cores = 0;
};

enum DISK_TYPE {
    DISK_HDD,
    DISK_SSD
};

struct disk_info {
    DISK_TYPE disk_type = DISK_SSD;
    uint64_t disk_total = 0;
    uint64_t disk_free = 0;
    uint64_t disk_awalible = 0;
    uint64_t disk_used = 0;
};

class SystemInfo : public Singleton<SystemInfo> {
public:
    SystemInfo();

    virtual ~SystemInfo();

    void start();

    void stop();

    OS_TYPE get_ostype() const {
        return m_os_type;
    }

    const mem_info& get_meminfo() const {
        return m_meminfo;
    }

    const cpu_info& get_cpuinfo() const {
        return m_cpuinfo;
    }

    const disk_info& get_diskinfo() const {
        return m_diskinfo;
    }

    float get_cpu_usage();

protected:
    void get_os_type();

    void get_mem_info(mem_info& info);

    void get_cpu_info(cpu_info& info);

    void get_disk_info(const std::string& path, disk_info& info);

    void update_cpu_usage();

private:
    OS_TYPE m_os_type = OS_1804;
    mem_info m_meminfo;
    cpu_info m_cpuinfo;
    disk_info m_diskinfo;

    float m_cpu_usage = 0.0f;
    std::thread* m_thread = nullptr;
    std::atomic<bool> m_running{false};
};

#endif //DBCPROJ_SYSTEM_INFO_H
