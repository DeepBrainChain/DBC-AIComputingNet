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
#include <sstream> 
#include <fstream>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <dirent.h>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "common/common.h"

#include "callback_wait.h"
#include "function_traits.h"
#include "singleton.h"
#include "rw_lock.h"
#include "RWMutex.h"
#include "LruCache.hpp"
#include "tweetnacl/tools.h"
#include "tweetnacl/randombytes.h"
#include "tweetnacl/tweetnacl.h"
#include "base64.h"

#include "utils/string_util.h"
#include "utils/file_util.h"
#include "utils/path_util.h"
#include "utils/url_util.h"
#include "utils/json_util.h"
#include "utils/crypt_util.h"
#include "utils/time_util.h"

#include "validator/ip_validator.h"
#include "validator/port_validator.h"
#include "validator/url_validator.h"

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

static const char* color_black = "\033[1;30m";
static const char* color_blue = "\033[1;34m";
static const char* color_red = "\033[1;31m";
static const char* color_green = "\033[1;32m";
static const char* color_reset = "\033[0m";

#define printf_black(...) \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s", color_black, line, color_reset); \
    } while(0);

#define printf_blue(...) \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s", color_blue, line, color_reset); \
    } while(0);

#define printf_red(...) \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s", color_red, line, color_reset); \
    } while(0);

#define printf_green(...) \
    do {                            \
    char line[1024] = {0}; \
    snprintf(line, 1024, __VA_ARGS__); \
    printf("%s%s%s", color_green, line, color_reset); \
    } while(0);

#define CALLBACK_0(__selector__,__target__, ...) std::bind(&__selector__,__target__, ##__VA_ARGS__)
#define CALLBACK_1(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, ##__VA_ARGS__)
#define CALLBACK_2(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, ##__VA_ARGS__)

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

std::string run_shell(const std::string& cmd, const char* modes = "r");

std::string run_shell2(const std::string& cmd);

// size: KB
std::string scale_size(uint64_t size, char* up = "BKMGTP\0");

inline std::string size2GB(uint64_t size) {
    return scale_size(size, "BKMG\0");
}

inline std::string size2MB(uint64_t size) {
    return scale_size(size, "BKM\0");
}

// 获取公网ip地址
std::string get_public_ip();

// 获取默认路由指定的网卡设备的IP地址
std::string get_default_route_ip();

// 隐藏ip地址，输出类似:*.51.*.142
std::string hide_ip_addr(std::string ip);

// string to digit
int32_t str_to_i32(const std::string& str);
uint32_t str_to_u32(const std::string& str);
int64_t str_to_i64(const std::string& str);
uint64_t str_to_u64(const std::string& str);
float str_to_f(const std::string& str);

inline std::string f2s(float f) {
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%.2f", f);
    std::stringstream ss;
    ss << atof(buf);
    snprintf(buf, sizeof(buf), "%s", ss.str().c_str());
    return ss.str();
}

void printhex(unsigned char *buf, int len);

void ByteToHexStr(const unsigned char* source, int sourceLen, char* dest);

void HexStrToByte(const char* source, int sourceLen, unsigned char* dest);

std::string encrypt_data(unsigned char* data, int32_t len, const std::string& pub_key, const std::string& priv_key);

bool decrypt_data(const std::string& data, const std::string& pub_key, const std::string& priv_key, std::string& ori_message);

#endif //DBCPROJ_UTILS_H
