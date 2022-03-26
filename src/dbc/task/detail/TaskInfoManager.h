#ifndef DBC_TASKINFO_MANAGER_H
#define DBC_TASKINFO_MANAGER_H

#include "util/utils.h"
#include "message/matrix_types.h"
#include "data/db/taskinfo_db.h"
#include "TaskResourceManager.h"

class TaskInfoManager : public Singleton<TaskInfoManager> {
public:
    TaskInfoManager() = default;

    virtual ~TaskInfoManager() = default;

    bool init();

    bool isExist(const std::string& task_id) {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_tasks.find(task_id);
        return it != m_tasks.end();
    }

    std::map<std::string, std::shared_ptr<dbc::TaskInfo> > getTasks() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_tasks;
    }

    std::vector<std::string> getAllTaskId() const {
        RwMutex::ReadLock rlock(m_mtx);
        std::vector<std::string> vTaskId;
        for (auto& it : m_tasks) {
            vTaskId.push_back(it.first);
        }

        return std::move(vTaskId);
    }

    std::shared_ptr<dbc::TaskInfo> getTaskInfo(const std::string& task_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_tasks.find(task_id);
        if (it != m_tasks.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void addTaskInfo(const std::shared_ptr<dbc::TaskInfo>& taskinfo) {
        RwMutex::WriteLock wlock(m_mtx);
        m_tasks[taskinfo->task_id] = taskinfo;
        m_db.write_task(taskinfo);
    }

    void delTaskInfo(const std::string& task_id) {
        RwMutex::WriteLock wlock(m_mtx);
        m_tasks.erase(task_id);
        m_db.delete_task(task_id);
    }

    void update(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
        RwMutex::WriteLock wlock(m_mtx);
        m_tasks[taskinfo->task_id] = taskinfo;
        m_db.write_task(taskinfo);
    }

private:
    mutable RwMutex m_mtx;
    TaskInfoDB m_db;
    std::map<std::string, std::shared_ptr<dbc::TaskInfo> > m_tasks;
};

typedef TaskInfoManager TaskInfoMgr;

#endif
