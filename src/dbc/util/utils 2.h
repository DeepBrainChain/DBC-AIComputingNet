#ifndef DBC_UTILS_H
#define DBC_UTILS_H

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <set>
#include <cassert>
#include <climits>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <algorithm>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <memory>
#include <chrono>
#include <thread>
#include <ctime>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>
#include <uuid/uuid.h>

#include "common/common.h"

#include "callback_wait.h"
#include "function_traits.h"
#include "singleton.h"
#include "rw_lock.h"

#include "utils/string_util.h"
#include "utils/file_util.h"
#include "utils/path_util.h"
#include "utils/url_util.h"
#include "utils/json_util.h"
#include "utils/crypt_util.h"

#include "validator/ip_validator.h"
#include "validator/port_validator.h"
#include "validator/url_validator.h"



//using namespace boost::asio::ip;

/**
* this function tries to raise the file descriptor limit to the requested number.
* It returns the actual file descriptor limit (which may be more or less than nMinFD)
*/
inline int32_t RaiseFileDescriptorLimit(int nMinFD)
{
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if(getrlimit(RLIMIT_NOFILE, &limitFD) != -1)
    {
        if(limitFD.rlim_cur < (rlim_t) nMinFD)
        {
            limitFD.rlim_cur = nMinFD;
            if(limitFD.rlim_cur > limitFD.rlim_max)
            {
                limitFD.rlim_cur = limitFD.rlim_max;
            }
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
    return nMinFD; // getrlimit failed, assume it's fine
#endif
}

inline void get_sys_mem(int64_t& mem, int64_t& mem_swap)
{
#if defined(WIN32)
    MEMORYSTATUSEX memoryInfo;
            memoryInfo.dwLength = sizeof(memoryInfo);
            GlobalMemoryStatusEx(&memoryInfo);
            mem = memoryInfo.ullTotalPhys;
            mem_swap = memoryInfo.ullTotalVirtual;
#elif defined(__linux__)
    struct sysinfo s_info;
    if (0 == sysinfo(&s_info))
    {
        mem = s_info.totalram;
        mem_swap = mem + s_info.totalswap;
    }
#endif
}

inline uint32_t get_disk_free(const std::string& disk_path)
{
#if defined(__linux__)
    struct statfs diskInfo;
    statfs(disk_path.c_str(), &diskInfo);
    uint64_t b_size = diskInfo.f_bsize;

    //uint64_t free = b_size * diskInfo.f_bfree;
    // general user can available
    uint64_t free = b_size * diskInfo.f_bavail;
    //MB
    free = free >> 20;
    return free;
#endif
    return 0;
}

inline uint32_t get_disk_total(const std::string& disk_path)
{
#if defined(__linux__)
    struct statfs diskInfo;
    statfs(disk_path.c_str(), &diskInfo);
    uint64_t b_size = diskInfo.f_bsize;
    uint64_t totalSize = b_size * (diskInfo.f_blocks - diskInfo.f_bfree + diskInfo.f_bavail);

    uint32_t total = totalSize >> 20;
    return total;
#endif
    return 0;
}


static std::string run_shell(const char* cmd, const char* modes = "r") {
    if (cmd == nullptr) return "";

    FILE * fp = nullptr;
    char buffer[1024] = {0};
    fp = popen(cmd, modes);
    if (fp != nullptr) {
        fgets(buffer, sizeof(buffer), fp);
        pclose(fp);
        return std::string(buffer);
    } else {
        return std::string("run_shell failed! ") + cmd + std::string(" ") + std::to_string(errno) + ":" + strerror(errno);
    }
}

static std::string read_info(const char* file, const char* modes = "r") {
    FILE *fp = fopen(file, modes);
    if(NULL == fp) return "";

    char szBuf[1024] = {0};
    fgets(szBuf, sizeof(szBuf) - 1, fp);
    fclose(fp);

    return std::string(szBuf);
}

// B => TB GB KB
std::string size_to_string(int64_t n, int64_t scale = 1);


#endif //DBCPROJ_UTILS_H
