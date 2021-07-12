#ifndef DBCPROJ_RESOURCEMANAGER_H
#define DBCPROJ_RESOURCEMANAGER_H

#include <iostream>
#include <string>

class ResourceManager {
public:
    // os
    static std::string os_name();

    // cpu
    static std::string cpu_type();
    static int32_t cpu_cores();

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
