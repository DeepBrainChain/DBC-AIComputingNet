#ifndef DBC_SYSTEM_INFO_H
#define DBC_SYSTEM_INFO_H

#include "utils.h"

// os
enum OS_TYPE {
    OS_Ubuntu_1804,
    OS_Ubuntu_2004
};

// cpu vendor
enum CPU_VENDOR_TYPE {
    Unknown = 0,
    Intel,
    AMD
};

// memory (KB)
struct mem_info {
    int64_t total = 0L;
    int64_t free = 0L;
    int64_t used = 0L;
    int64_t available = 0L;
    int64_t buffers = 0L;
    int64_t cached = 0L;
    int64_t swap_total = 0L;
    int64_t swap_free = 0L;
    int64_t shared = 0L;
    float usage = 0.0f;
};

// cpu
struct cpu_info {
    std::string cpu_name;
    std::string mhz;
    int32_t cores = 0;
    int32_t physical_cpus = 0;
    int32_t physical_cores_per_cpu = 0;
    int32_t logical_cores_per_cpu = 0;
    int32_t threads_per_cpu = 0;
    CPU_VENDOR_TYPE vendor_type = CPU_VENDOR_TYPE::Unknown;
};

// gpu
struct gpu_info {
    std::string id;
    std::list<std::string> devices;

    static std::string parse_bus(const std::string &id);

    static std::string parse_slot(const std::string &id);

    static std::string parse_function(const std::string &id);
};

// disk
enum DISK_TYPE {
    DISK_HDD,
    DISK_SSD
};

// KB
struct disk_info {
    DISK_TYPE disk_type = DISK_SSD;
    uint64_t total = 0;
    uint64_t free = 0;
    uint64_t available = 0;
    float usage = 0.0f;
    std::string data_mount_status;
};

class SystemInfo : public Singleton<SystemInfo> {
public:
    SystemInfo();

    virtual ~SystemInfo();

    ERRCODE Init(NODE_TYPE node_type, int32_t reserved_cpu_cores = 0, int32_t reserved_memory = 0);

    void exit();

    std::string GetPublicip() const {
        return m_public_ip;
    }

    std::string GetDefaultRouteIp() const {
        return m_default_route_ip;
    }

    OS_TYPE GetOsType() const {
        return m_os_type;
    }

    std::string GetOsName() const {
        return m_os_name;
    }

    const mem_info &GetMemInfo() const {
        RwMutex::ReadLock rlock(m_mem_mtx);
        return m_meminfo;
    }

    float getMemUsage() const {
        RwMutex::ReadLock rlock(m_mem_mtx);
        return m_meminfo.usage;
    }

    const cpu_info &GetCpuInfo() const {
        return m_cpuinfo;
    }

    float GetCpuUsage() const {
        RwMutex::ReadLock rlock(m_cpu_mtx);
        return m_cpu_usage;
    }

    const std::map<std::string, gpu_info>& GetGpuInfo() const {
        RwMutex::ReadLock rlock(m_gpu_mtx);
        return m_gpuinfo;
    }

    const std::map<std::string, disk_info>& GetDiskInfo() const {
        RwMutex::ReadLock rlock(m_disk_mtx);
        return m_diskinfos;
    }

    void GetDiskInfo(const std::string& path, disk_info& info);

    const std::vector<float> &loadaverage() const {
        RwMutex::ReadLock rlock(m_loadaverage_mtx);
        return m_loadaverage;
    }

protected:
    void init_os_type();

    void update_mem_info();

    void update_cpu_info();

    void update_gpu_info();

    void update_disk_info();

    void update_disk_info(const std::string &path, disk_info &info);

    void update_load_average(std::vector<float> &average);

    void update_thread_func();

private:
    NODE_TYPE m_node_type = NODE_TYPE::ComputeNode;
    
    // os
    OS_TYPE m_os_type = OS_TYPE::OS_Ubuntu_1804;
    std::string m_os_name;
    
    // memory
    mutable RwMutex m_mem_mtx;
    mem_info m_meminfo;
    
    // cpu
    mutable RwMutex m_cpu_mtx;
    cpu_info m_cpuinfo;
    float m_cpu_usage = 0.0f;
    
    // gpu: <id, gpu_info>
    mutable RwMutex m_gpu_mtx;
    std::map<std::string, gpu_info> m_gpuinfo;

    // disk: /data /data2 /data3 ...
    mutable RwMutex m_disk_mtx;
    std::map<std::string, disk_info> m_diskinfos;

    // load average
    std::vector<float> m_loadaverage;
    mutable RwMutex m_loadaverage_mtx;

	std::string m_public_ip;
	std::string m_default_route_ip;
    
    int32_t m_reserved_cpu_cores = 0;
    int32_t m_reserved_memory = 0;

    std::thread *m_thread_update = nullptr;
    std::atomic<bool> m_running {false};
};

#endif
