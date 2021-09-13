#ifndef DBC_TASK_DEF_H
#define DBC_TASK_DEF_H

#include <iostream>
#include <string>

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

enum ETaskStatus {
    TS_None,
    TS_Creating,    //正在创建
    TS_Running,     //正在运行中
    TS_Starting,    //正在启动
    TS_Stopping,    //正在停止
    TS_Restarting,  //正在重启
    TS_Resetting,   //正在reset重启
    TS_Deleting,    //正在删除
    TS_PullingImageFile    //正在下载镜像文件
};

enum ETaskOp {
    T_OP_None,
    T_OP_Create,    //创建
    T_OP_Start,     //启动
    T_OP_Stop,      //停止
    T_OP_ReStart,   //重启
    T_OP_Reset,     //reset重启
    T_OP_Delete,   //删除
    T_OP_PullImageFile   //下载镜像文件
};

enum EVmStatus {
    VS_SHUT_OFF,
    VS_PAUSED,
    VS_RUNNING
};

enum ETaskLogDirection {
    LD_Head,
    LD_Tail
};

std::string task_status_string(int32_t status);

// 系统盘大小（GB）
static const int32_t g_disk_system_size = 350;
// 内存预留大小（GB）
static const int32_t g_reserved_memory = 32;
// 虚拟机登录用户名
static const char* g_vm_login_username = "dbc";

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
