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


enum ETaskOp {
    T_OP_None,
    T_OP_Create,    //创建
    T_OP_Start,     //启动
    T_OP_Stop,      //停止
    T_OP_ReStart,   //重启(reboot domain)
    T_OP_ForceReboot,  //强制重启(destroy domain && start domain)
    T_OP_Reset,     //reset
    T_OP_Delete,    //删除
    T_OP_CreateSnapshot  //创建快照
};

struct ETaskEvent {
    std::string task_id;
    ETaskOp op;
    std::string image_server;
};

enum ETaskStatus {
    TS_ShutOff,     //关闭状态
    TS_Creating,    //正在创建
    TS_Running,     //正在运行中
    TS_Starting,    //正在启动
    TS_Stopping,    //正在停止
    TS_Restarting,  //正在重启
    TS_Resetting,   //正在reset
    TS_Deleting,    //正在删除
    TS_CreatingSnapshot,  //正在创建快照
    TS_PMSuspended, //虚拟机已进入睡眠状态

    TS_Error = 100,   //出错
    TS_CreateError,
    TS_StartError,
    TS_StopError,
    TS_RestartError,
    TS_ResetError,
    TS_DeleteError,
    TS_CreateSnapshotError
};

std::string task_status_string(int32_t status);
std::string task_operation_string(int32_t op);
std::string vm_status_string(virDomainState status);


struct TaskCreateParams {
    std::string task_id;
    std::string login_password;
    std::string image_name;     // system disk image name
    std::string custom_image_name;
    std::string data_file_name; // data disk file name
    int16_t ssh_port;
    int16_t rdp_port;
    std::vector<std::string> custom_port;
    int32_t cpu_sockets = 0;
    int32_t cpu_cores = 0;
    int32_t cpu_threads = 0;

    std::map<std::string, std::list<std::string>> gpus;

    uint64_t mem_size; // KB

    std::map<int32_t, uint64_t> disks; //数据盘(单位：KB)，index从1开始

    std::string vm_xml;
    std::string vm_xml_url;

    int16_t vnc_port;
    std::string vnc_password;

    std::string operation_system; //操作系统(如generic, ubuntu 18.04, windows 10)，默认ubuntu，带有win则认为是windows系统，必须全小写。
    std::string bios_mode; //BIOS模式(如legacy,uefi)，默认传统BIOS，必须全小写。

    std::vector<std::string> multicast; //组播地址(如："230.0.0.1:5558")

    std::string network_name;
    std::string vxlan_vni;
};

struct ParseVmXmlParams {
    std::string task_id;
    std::string image_name;
    std::string data_file_name;

    int32_t cpu_sockets;
    int32_t cpu_cores;
    int32_t cpu_threads;

    std::map<std::string, std::list<std::string>> gpus;

    uint64_t mem_size; // KB

    std::map<int32_t, uint64_t> disks; //数据盘(单位：KB)，index从1开始

    int16_t vnc_port;
    std::string vnc_password;

    std::string operation_system; //操作系统(如generic, ubuntu 18.04, windows 10)，默认ubuntu，带有win则认为是windows系统，必须全小写。
    std::string bios_mode; //BIOS模式(如legacy,uefi)，默认传统BIOS，必须全小写。

    std::vector<std::string> multicast; //组播地址(如："230.0.0.1:5558")
};

#endif
