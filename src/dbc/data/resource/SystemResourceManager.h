#ifndef DBC_SYSTEMRESOURCEMANAGER_H
#define DBC_SYSTEMRESOURCEMANAGER_H

#include "util/utils.h"

class SystemResourceManager : public Singleton<SystemResourceManager> {
public:
    SystemResourceManager() = default;

    virtual ~SystemResourceManager() = default;

    // os、cpu、gpu、mem、disk
    void Init();

    const OS& GetOS() {
        return m_os;
    }

    const DeviceCpu& GetCpu() {
        return m_cpu;
    }

    const std::map<std::string, DeviceGpu>& GetGpu() {
        return m_gpu;
    }

    const DeviceMem& GetMem() {
        return m_mem;
    }

    const DeviceDisk& GetDisk() {
        return m_disk;
    }

    // test
    void print_os();
    void print_cpu();
    void print_gpu();
    void print_mem();
    void print_disk();

private:
    void init_os();
    void init_cpu();
    void init_gpu();
    void init_mem();
    void init_disk();

    // os
    std::string os_name();
    std::string os_version();

    // cpu
    std::string cpu_type();
    int32_t cpu_cores();
    int32_t physical_cpu();
    int32_t cpu_cores_per_physical();
    int32_t cpu_siblings_per_physical();
    int32_t cpu_threads();

    // mem (MB)
    int64_t mem_total();
    int64_t mem_used();
    int64_t mem_free();
    int64_t mem_buff_cache();
    int64_t mem_available();

    // disk (MB)
    std::string disk_type(const std::string& path = "/data");
    int64_t disk_total(const std::string& path = "/data");
    int64_t disk_used(const std::string& path = "/data");
    int64_t disk_available(const std::string& path = "/data");

private:
    OS m_os;
    DeviceCpu m_cpu;
    std::map<std::string, DeviceGpu> m_gpu; // <id, gpu>
    DeviceMem m_mem;
    DeviceDisk m_disk;
};

typedef SystemResourceManager SystemResourceMgr;

#endif //DBCPROJ_SYSTEMRESOURCEMANAGER_H
