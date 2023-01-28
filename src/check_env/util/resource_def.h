#ifndef DBC_RESOURCE_DEF_H
#define DBC_RESOURCE_DEF_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <list>

struct OS {
    std::string name;
    std::string version;
};

struct DeviceCpu {
    int32_t sockets = 0;
    int32_t cores_per_socket = 0;
    int32_t threads_per_core = 0;

    int32_t total_cores() const;
};

struct DeviceGpu {
    std::string id;
    std::list<std::string> devices;

    static std::string parse_bus(const std::string& id);
    static std::string parse_slot(const std::string& id);
    static std::string parse_function(const std::string& id);
};

struct DeviceMem {
    int64_t total = 0;
    int64_t available = 0;
};

enum DiskType {
    DT_HDD,
    DT_SSD
};

struct DeviceDisk {
    DiskType type = DT_SSD;
    int64_t total = 0;
    int64_t available = 0;
};

#endif //DBC_RESOURCE_DEF_H
