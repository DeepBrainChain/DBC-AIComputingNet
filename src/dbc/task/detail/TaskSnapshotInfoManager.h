#ifndef DBC_SNAPSHOT_MANAGER_H
#define DBC_SNAPSHOT_MANAGER_H

#include "util/utils.h"
#include "db/db_types/task_snapshotinfo_types.h"

/**
 * m_creating_snapshot 存储正在执行的快照任务和执行失败的快照任务，每个虚拟机只有一个。
 * m_task_snapshot 存储成功的快照，一个虚拟机可以有多个快照。
 * 
 * 新添加的快照任务的error_code 和 error_message没有值，即dbc::snapshotInfo.__isset.error_code = false。
 * 成功的快照任务error_code 为 0, error_message 可以为空字符串，可以没有值。
 * 失败的快照任务error_code 为 非零整数， error_message 为失败原因。
 */

class SnapshotManager : public Singleton<SnapshotManager> {
public:
    SnapshotManager() = default;
    virtual ~SnapshotManager() = default;

    bool init(const std::vector<std::string>& taskids);

    std::shared_ptr<dbc::snapshotInfo> getCreatingSnapshot(const std::string& task_id) const;

    void addCreatingSnapshot(const std::string& task_id, const std::shared_ptr<dbc::snapshotInfo>& snapshot);

    void delCreatingSnapshot(const std::string& task_id);

    void updateCreatingSnapshot(const std::string& task_id, int32_t error_code, const std::string& error_message);

    void listTaskSnapshot(const std::string& task_id, std::vector<std::shared_ptr<dbc::snapshotInfo>>& snaps, bool includeCreating = true) const;

    int32_t getTaskSnapshotCount(const std::string& task_id) const;

    std::shared_ptr<dbc::snapshotInfo> getTaskSnapshot(const std::string& task_id, const std::string& snapshot_name, bool includeCreating = true) const;

    void addTaskSnapshot(const std::string& task_id, const std::shared_ptr<dbc::snapshotInfo>& snapshot);

    void delTaskSnapshot(const std::string& task_id);

public:
    struct taskSnapshotIndex {
        bool operator < (const taskSnapshotIndex& other) const {
            if (task_id != other.task_id) return task_id < other.task_id;
            return snapshot_name < other.snapshot_name;
        }
        std::string task_id;
        std::string snapshot_name;
    };

private:
    mutable RwMutex m_mtx;
    std::map<std::string, std::shared_ptr<dbc::snapshotInfo> > m_creating_snapshot;
    std::map<taskSnapshotIndex, std::shared_ptr<dbc::snapshotInfo>> m_task_snapshot;
};

#endif
