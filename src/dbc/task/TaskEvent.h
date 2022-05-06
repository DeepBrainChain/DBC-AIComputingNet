#ifndef DBC_TASK_EVENT_H
#define DBC_TASK_EVENT_H

#include "util/utils.h"

enum class TaskEventType {
    TET_Unknown,

    TET_CreateTask,         //创建Task
    TET_StartTask,          //启动Task
    TET_ShutdownTask,       //关闭Task
    TET_PowerOffTask,       //强制断电
    TET_ReStartTask,        //重启Task (reboot domain)
    TET_ForceRebootTask,    //强制重启Task (destroy domain && start domain)
    TET_ResetTask,          //Reset Task
    TET_DeleteTask,         //删除Task
    TET_ModifyTask,         //修改task

    TET_ResizeDisk,         //磁盘扩容
    TET_AddDisk,            //添加磁盘
    TET_DeleteDisk,         //删除磁盘

    TET_CreateSnapshot      //创建快照
};

struct TaskEvent {
    TaskEvent() = default;
    virtual ~TaskEvent() = default;

    TaskEventType type = TaskEventType::TET_Unknown;
    std::string task_id;
};

/*********** task ***********/

struct CreateTaskEvent : public TaskEvent {
    CreateTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_CreateTask;
        task_id = _task_id;
    }
};

struct StartTaskEvent : public TaskEvent { 
    StartTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_StartTask;
        task_id = _task_id;
    }
};

struct ShutdownTaskEvent : public TaskEvent {
    ShutdownTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ShutdownTask;
        task_id = _task_id;
    }
};

struct PowerOffTaskEvent : public TaskEvent {
    PowerOffTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_PowerOffTask;
        task_id = _task_id;
    }
};

struct ReStartTaskEvent : public TaskEvent {
    ReStartTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ReStartTask;
        task_id = _task_id;
    }
};

struct ForceRebootTaskEvent : public TaskEvent {
    ForceRebootTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ForceRebootTask;
        task_id = _task_id;
    }
};

struct ResetTaskEvent : public TaskEvent {
    ResetTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ResetTask;
        task_id = _task_id;
    }
};

struct DeleteTaskEvent : public TaskEvent {
    DeleteTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_DeleteTask;
        task_id = _task_id;
    }
};

struct ModifyTaskEvent : public TaskEvent {
    ModifyTaskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ModifyTask;
        task_id = _task_id;
    }
};

/*********** disk ***********/

struct ResizeDiskEvent : public TaskEvent {
    ResizeDiskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_ResizeDisk;
        task_id = _task_id;
    }

    std::string disk_name;      //磁盘名称
    int64_t size_k = 0;         //扩容大小（KB）
    std::string source_file;    //文件绝对路径
};

struct AddDiskEvent : public TaskEvent {
    AddDiskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_AddDisk;
        task_id = _task_id;
    }

    int64_t size_k = 0;     //新加磁盘大小（KB）
    std::string mount_dir;  //挂载目录
};

struct DeleteDiskEvent : public TaskEvent {
    DeleteDiskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_DeleteDisk;
        task_id = _task_id;
    }

    std::string disk_name;  //磁盘名称
};

/*********** snapshot ***********/

struct CreateAndUploadSnapshotEvent : public TaskEvent {
    CreateAndUploadSnapshotEvent(const std::string& _task_id) {
        type = TaskEventType::TET_CreateSnapshot;
        task_id = _task_id;
    }

    std::string snapshot_name;  //快照名称
    std::string root_backfile;  //root backfile
    std::string source_file;    //要做快照的磁盘绝对路径
    std::string snapshot_file;  //快照文件
    int64_t create_time = 0;    //创建时间
    ImageServer image_server;   //上传的镜像中心
};

#endif