#ifndef DBC_TASK_DISK_MANAGER_H
#define DBC_TASK_DISK_MANAGER_H

#include "util/utils.h"
#include "task/TaskEvent.h"
#include "DiskInfo.h"
#include "SnapshotInfo.h"

class TaskDiskManager : public Singleton<TaskDiskManager> {
public:
    friend class TaskManager;

    TaskDiskManager() = default;

    virtual ~TaskDiskManager() = default;

    FResult init(const std::vector<std::string>& task_ids);

	// 删除task的所有磁盘（包括删除磁盘文件）
	void del_disks(const std::string& task_id);
    
    void add_disk(const std::string& task_id, const std::shared_ptr<DiskInfo>& diskinfo);


    // 磁盘列表
    void listDisks(const std::string& task_id, std::map<std::string, std::shared_ptr<DiskInfo> >& disks);

    // 磁盘扩容（KB）
    FResult resizeDisk(const std::string& task_id, const std::string& disk_name, int64_t size_k);

    // 添加新数据盘
    FResult addDisk(const std::string& task_id, int64_t size_k, const std::string& mount_dir = "/data");

    // 删除数据盘（不能删除 vda 系统盘）
    FResult deleteDisk(const std::string& task_id, const std::string& disk_name);

    
    // 删除task的所有快照（包括删除文件）
    void del_snapshots(const std::string& task_id);

    // 快照列表
    void listSnapshots(const std::string& task_id, std::vector<dbc::snapshot_info>& snapshots);

    // 创建快照并上传
    FResult createAndUploadSnapshot(const std::string& task_id, const std::string& snapshot_name, 
        const ImageServer& image_server, const std::string& desc);

    // 快照上传进度
    float uploadSnapshotProgress(const std::string& task_id, const std::string& snapshot_name);

    // 终止镜像上传
    void terminateUploadSnapshot(const std::string& task_id, const std::string& snapshot_name);

protected:
    void process_resize_disk(const std::shared_ptr<TaskEvent>& ev);

    void process_add_disk(const std::shared_ptr<TaskEvent>& ev);

    void process_delete_disk(const std::shared_ptr<TaskEvent>& ev);

    void process_create_snapshot(const std::shared_ptr<TaskEvent>& ev);

private:
    mutable RwMutex m_disk_mtx;
    std::map<std::string, std::map<std::string, std::shared_ptr<DiskInfo> > > m_task_disks;
    std::map<std::string, std::string> m_vda_root_backfile;

    mutable RwMutex m_snapshot_mtx;
    TaskSnapshotDB m_snapshot_db;
    std::map<std::string, std::shared_ptr<SnapshotInfo>> m_task_snapshots;
};

typedef TaskDiskManager TaskDiskMgr;

#endif
