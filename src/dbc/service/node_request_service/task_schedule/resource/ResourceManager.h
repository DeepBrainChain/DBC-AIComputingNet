#ifndef DBCPROJ_RESOURCEMANAGER_H
#define DBCPROJ_RESOURCEMANAGER_H

#include <iostream>
#include <string>
#include "singleton.h"
#include <vector>
#include <map>
#include <list>

struct DeviceGpu {
    std::string id;
    std::vector<std::string> devices;

    static std::string parse_bus(const std::string& id);
    static std::string parse_slot(const std::string& id);
    static std::string parse_function(const std::string& id);
};

struct DeviceCpu {
    int32_t sockets;
    int32_t cores;
    int32_t threads;
};

enum DiskType {
    DT_HDD,
    DT_SSD
};

struct DeviceDisk {
    DiskType type;
    int64_t total;
    int64_t available;
};

struct DeviceMem {
    int64_t total;
    int64_t available;
};

class HardwareManager : public Singleton<HardwareManager> {
public:
    HardwareManager() = default;

    virtual ~HardwareManager() = default;

    // cpu、gpu、disk、mem
    void Init();


    // test
    void print_gpu();

private:
    void init_gpu();
    void init_cpu();
    void init_disk();
    void init_mem();

private:
    // system
    DeviceCpu m_cpu;
    std::map<std::string, DeviceGpu> m_gpu; // <id, gpu>
    DeviceMem m_mem;
    DeviceDisk m_disk;

    // task
    std::map<std::string, DeviceCpu> m_mpTaskCpu;
    std::map<std::string, std::list<DeviceGpu>> m_mpTaskGpu;
    std::map<std::string, DeviceMem> m_mpTaskMem;
    std::map<std::string, DeviceDisk> m_mpTaskDisk;
};

class ResourceManager {
public:
    // os
    static std::string os_name();

    // cpu
    static std::string cpu_type();
    static int32_t cpu_cores();
    static int32_t physical_cpu();
    static int32_t cpu_cores_per_physical();
    static int32_t cpu_siblings_per_physical();
    static int32_t cpu_threads();

    // mem (G)
    static int32_t mem_total();
    static int32_t mem_used();
    static int32_t mem_free();
    static int32_t mem_buff_cache();

    // disk (G)
    static int32_t disk_size(const std::string& path = "/data");
    static int32_t disk_used(const std::string& path = "/data");
    static int32_t disk_free(const std::string& path = "/data");
    static std::string disk_type(const std::string& path = "/data");

    // gpu
    static int32_t gpu_count();

};

typedef ResourceManager ResourceMgr;

#endif //DBCPROJ_RESOURCEMANAGER_H
