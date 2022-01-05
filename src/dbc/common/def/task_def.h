#ifndef DBC_TASK_DEF_H
#define DBC_TASK_DEF_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#define MAX_LIST_TASK_COUNT                                     10000
#define MAX_TASK_COUNT_PER_REQ                                  10

#define MAX_TASK_COUNT_ON_PROVIDER                              10000
#define MAX_TASK_COUNT_IN_TRAINING_QUEUE                        1000
#define MAX_TASK_SHOWN_ON_LIST                                  100

#define GET_LOG_HEAD                                            0
#define GET_LOG_TAIL                                            1

#define MAX_NUMBER_OF_LINES                                     100
#define DEFAULT_NUMBER_OF_LINES                                 100

#define MAX_LOG_CONTENT_SIZE                                    (8 * 1024)

#define MAX_ENTRY_FILE_NAME_LEN                                 128
#define MAX_ENGINE_IMGE_NAME_LEN                                128

#define AI_TRAINING_MAX_TASK_COUNT                                  64

enum ETaskOp {
    T_OP_None,
    T_OP_Create,    //创建
    T_OP_Start,     //启动
    T_OP_Stop,      //停止
    T_OP_ReStart,   //重启
    T_OP_Reset,     //reset
    T_OP_Delete,    //删除
    T_OP_CreateSnapshot  //创建快照
};

struct ETaskEvent {
    std::string task_id;
    ETaskOp op;
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

    TS_Error,   //出错
    TS_CreateError,
    TS_StartError,
    TS_StopError,
    TS_RestartError,
    TS_ResetError,
    TS_DeleteError,
    TS_CreateSnapshotError
};

enum ETaskLogDirection {
    LD_Head,
    LD_Tail
};

std::string task_status_string(int32_t status);
std::string task_operation_string(int32_t op);
std::string vm_status_string(virDomainState status);

// 系统盘大小（GB）
static const int32_t g_disk_system_size = 350;
// 内存预留大小（GB）
static const int32_t g_reserved_memory = 32;
// cpu预留核数（每个物理cpu的物理核数）
static const int32_t g_reserved_physical_cores_per_cpu = 1;
// 虚拟机登录用户名
static const char* g_vm_ubuntu_login_username = "dbc";
static const char* g_vm_windows_login_username = "Administrator";


enum MACHINE_STATUS {
    MS_NONE,
    MS_VERIFY, //验证阶段
    MS_ONLINE, //验证完上线阶段
    MS_RENNTED //租用中
};

enum USER_ROLE {
    UR_NONE,
    UR_VERIFIER,
    UR_RENTER_WALLET,
    UR_RENTER_SESSION_ID
};

struct TaskCreateParams {
    std::string task_id;
    std::string login_password;
    std::string image_name;     // system disk image name
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

/*
enum TASK_STATE {
    DBC_TASK_RUNNING,
    DBC_TASK_NULL,
    DBC_TASK_NOEXIST,
    DBC_TASK_STOPPED
};

enum training_task_status
{
    task_status_unknown = 1,
    task_status_queueing = 2,
    task_status_pulling_image = 3,
    task_status_running = 4,
    task_status_update_error = 5,  //代表从running的容器升级
    task_status_creating_image = 6,  //正在创建镜像
    //stop_update_task_error  =7,   //代表从stop的容器升级
    //termianl state
    task_status_stopped = 8,
    task_successfully_closed = 16,
    task_abnormally_closed = 32,
    task_overdue_closed = 64,
    task_noimage_closed = 65,
    task_nospace_closed = 66,
    task_status_out_of_gpu_resource = 67
};

std::string to_training_task_status_string(int8_t status);
bool check_task_engine(const std::string& engine);
void set_task_engine(std::string engine);
*/

#endif //DBC_TASK_DEF_H
