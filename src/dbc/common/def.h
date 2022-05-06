#ifndef DBC_DEF_H
#define DBC_DEF_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <cassert>
#include <memory>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <boost/process.hpp>

#define MAX_NUMBER_OF_LINES         100
#define MAX_LOG_CONTENT_SIZE        (8 * 1024)

#define SERVICE_NAME_AI_TRAINING    "ai_training"


// 系统盘大小（GB）
static const int32_t g_disk_system_size = 350;
// 内存预留大小（GB）
static const int32_t g_reserved_memory = 32;
// cpu预留核数（每个物理cpu的物理核数）
static const int32_t g_reserved_physical_cores_per_cpu = 1;
// 虚拟机登录用户名
static const char* g_vm_ubuntu_login_username = "dbc";
static const char* g_vm_windows_login_username = "Administrator";


enum class QUERY_PEERS_FLAG {
    Active,
    Global
};

enum class QUERY_LOG_DIRECTION {
    Head,
    Tail
};

enum class NODE_TYPE {
    ComputeNode,
    ClientNode,
    SeedNode
};

enum class MACHINE_STATUS {
    Unknown,
    Verify,
    Online,
    Rented
};

enum class USER_ROLE {
    Unknown,
    Verifier,
    WalletRenter,
    SessionIdRenter
};

struct AuthoriseResult {
    bool success = false;
    std::string errmsg;
    MACHINE_STATUS machine_status = MACHINE_STATUS::Unknown;
    USER_ROLE user_role = USER_ROLE::Unknown;
    std::string rent_wallet;
    int64_t rent_end = 0;
};

std::string vm_status_string(virDomainState status);

struct ImageFile {
    std::string name;
    int64_t size;  //B

    bool operator == (const ImageFile& other) const {
        return this->name == other.name && this->size == other.size;
    }

    bool operator < (const ImageFile& other) const {
        if (this->name != other.name) {
            return this->name < other.name;
        }
        else {
            return this->size < other.size;
        }
    }
};

struct ImageServer {
    std::string ip;
    std::string port = "873";
    std::string modulename = "images";
    std::string id;

    std::string to_string();
    void from_string(const std::string& str);
};

#endif
