#ifndef DBC_TASK_EVENT_H
#define DBC_TASK_EVENT_H

#include "util/utils.h"

enum class TaskEventType {
    TET_Unknown,

    TET_CreateTask,         //����Task
    TET_StartTask,          //����Task
    TET_ShutdownTask,       //�ر�Task
    TET_PowerOffTask,       //ǿ�ƶϵ�
    TET_ReStartTask,        //����Task (reboot domain)
    TET_ForceRebootTask,    //ǿ������Task (destroy domain && start domain)
    TET_ResetTask,          //Reset Task
    TET_DeleteTask,         //ɾ��Task
    TET_ModifyTask,         //�޸�task

    TET_ResizeDisk,         //��������
    TET_AddDisk,            //��Ӵ���
    TET_DeleteDisk,         //ɾ������

    TET_CreateSnapshot      //��������
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

    std::string disk_name;      //��������
    int64_t size_k = 0;         //���ݴ�С��KB��
    std::string source_file;    //�ļ�����·��
};

struct AddDiskEvent : public TaskEvent {
    AddDiskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_AddDisk;
        task_id = _task_id;
    }

    int64_t size_k = 0;     //�¼Ӵ��̴�С��KB��
    std::string mount_dir;  //����Ŀ¼
};

struct DeleteDiskEvent : public TaskEvent {
    DeleteDiskEvent(const std::string& _task_id) {
        type = TaskEventType::TET_DeleteDisk;
        task_id = _task_id;
    }

    std::string disk_name;  //��������
};

/*********** snapshot ***********/

struct CreateAndUploadSnapshotEvent : public TaskEvent {
    CreateAndUploadSnapshotEvent(const std::string& _task_id) {
        type = TaskEventType::TET_CreateSnapshot;
        task_id = _task_id;
    }

    std::string snapshot_name;  //��������
    std::string root_backfile;  //root backfile
    std::string source_file;    //Ҫ�����յĴ��̾���·��
    std::string snapshot_file;  //�����ļ�
    int64_t create_time = 0;    //����ʱ��
    ImageServer image_server;   //�ϴ��ľ�������
};

#endif