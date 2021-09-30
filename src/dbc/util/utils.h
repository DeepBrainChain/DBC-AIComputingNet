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
#include <sys/types.h>
#include <dirent.h>

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

#include "validator/ip_validator.h"
#include "validator/port_validator.h"
#include "validator/url_validator.h"

namespace fs = boost::filesystem;

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

inline uint64_t power(unsigned int base, unsigned int expo)
{
    return (expo == 0) ? 1 : base * power(base, expo - 1);
}

// size: KB
static const char *scale_size(uint64_t size)
{
    static char up[] = { 'B', 'K', 'M', 'G', 'T', 'P', 0 };
    static char buf[BUFSIZ];
    int i;
    uint32_t base = 1024;
    uint64_t bytes = size * 1024LU;

    if (4 >= snprintf(buf, sizeof(buf), "%lu%c", bytes, up[0]))
        return buf;

    for (i = 1; up[i] != 0; i++) {
        if (4 >= snprintf(buf, sizeof(buf), "%.1f%c",
                          (float)(bytes * 1.0 / power(base, i)), up[i]))
            return buf;
        if (4 >= snprintf(buf, sizeof(buf), "%lu%c",
                          (uint64_t)(bytes * 1.0 / power(base, i)), up[i]))
            return buf;
    }

    return buf;
}

// 获取公网ip地址
std::string get_public_ip();

// string to digit
int32_t str_to_i32(const std::string& str);
uint32_t str_to_u32(const std::string& str);
int64_t str_to_i64(const std::string& str);
uint64_t str_to_u64(const std::string& str);
float str_to_f(const std::string& str);

void printhex(unsigned char *buf, int len);

void ByteToHexStr(const unsigned char* source, int sourceLen, char* dest);

void HexStrToByte(const char* source, int sourceLen, unsigned char* dest);

std::string encrypt_data(unsigned char* data, int32_t len, const std::string& pub_key, const std::string& priv_key);

bool decrypt_data(const std::string& data, const std::string& pub_key, const std::string& priv_key, std::string& ori_message);

#endif //DBCPROJ_UTILS_H
