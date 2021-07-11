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
    static int32_t disk_size();
    static int32_t disk_used();
    static int32_t disk_free();
    static std::string disk_type();

};

typedef ResourceManager ResourceMgr;

#endif //DBCPROJ_RESOURCEMANAGER_H
