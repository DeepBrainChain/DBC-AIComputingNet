#ifndef DBCPROJ_SYSTEM_INFO_H
#define DBCPROJ_SYSTEM_INFO_H

#include "utils.h"

namespace bpo = boost::program_options;

enum OS_TYPE {
    OS_1804,
    OS_2004
};

// KB
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
    std::string mhz;
    int32_t physical_cores = 0;
    int32_t physical_cores_per_cpu = 0;
    int32_t logical_cores_per_cpu = 0;
    int32_t threads_per_cpu = 0;
    int32_t total_cores = 0;
};

struct gpu_info {
    std::string id;
    std::list<std::string> devices;

    static std::string parse_bus(const std::string &id);

    static std::string parse_slot(const std::string &id);

    static std::string parse_function(const std::string &id);
};

enum DISK_TYPE {
    DISK_HDD,
    DISK_SSD
};

// KB
struct disk_info {
    DISK_TYPE disk_type = DISK_SSD;
    uint64_t disk_total = 0;
    uint64_t disk_free = 0;
    uint64_t disk_available = 0;
    float disk_usage = 0.0f;
};

class SystemInfo : public Singleton<SystemInfo> {
public:
    SystemInfo();

    virtual ~SystemInfo();

    void init(bpo::variables_map &options, int32_t reserved_cpu_cores, int32_t reserved_memory);

    void start();

    void stop();

    std::string publicip() const {
        return m_public_ip;
    }

    std::string defaultRouteIp() const {
        return m_default_route_ip;
    }

    OS_TYPE ostype() const {
        return m_os_type;
    }

    std::string osname() const {
        return m_os_name;
    }

    const mem_info &meminfo() const {
        return m_meminfo;
    }

    const cpu_info &cpuinfo() const {
        return m_cpuinfo;
    }

    float cpu_usage() const {
        return m_cpu_usage;
    }

    const std::map<std::string, gpu_info> &gpuinfo() const {
        return m_gpuinfo;
    }

    const disk_info &diskinfo() const {
        return m_diskinfo;
    }

protected:
    void init_os_type();

    void update_mem_info(mem_info &info);

    void init_cpu_info(cpu_info &info);

    void init_gpu();

    void update_disk_info(const std::string &path, disk_info &info);

    void update_func();

private:
    bool m_is_compute_node = false;

    OS_TYPE m_os_type = OS_1804;
    std::string m_os_name = "N/A";
    mem_info m_meminfo;
    cpu_info m_cpuinfo;
    std::map<std::string, gpu_info> m_gpuinfo; // <id, gpu>
    disk_info m_diskinfo;

    float m_cpu_usage = 0.0f;
    std::thread *m_thread = nullptr;
    std::atomic<bool> m_running{false};

    std::string m_public_ip = "N/A";
    std::string m_default_route_ip = "N/A";

    int32_t m_reserved_cpu_cores = 0; // 每个物理cpu的物理核数
    int32_t m_reserved_memory = 0; // GB
};

#endif //DBCPROJ_SYSTEM_INFO_H
