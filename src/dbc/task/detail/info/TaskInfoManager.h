#ifndef DBC_TASK_INFO_MANAGER_H
#define DBC_TASK_INFO_MANAGER_H

#include "util/utils.h"
#include "message/matrix_types.h"
#include "db/task_info_db.h"
#include "db/running_task_db.h"
#include "TaskInfo.h"

class TaskInfoManager : public Singleton<TaskInfoManager> {
public:
    friend class TaskManager;

    TaskInfoManager() = default;

    virtual ~TaskInfoManager() = default;

    FResult init();

    bool isExist(const std::string& task_id) {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_task_infos.find(task_id);
        return it != m_task_infos.end();
    }

    std::map<std::string, std::shared_ptr<TaskInfo> > getAllTaskInfos() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_task_infos;
    }

    std::vector<std::string> getAllTaskIds() const {
        RwMutex::ReadLock rlock(m_mtx);
        std::vector<std::string> ids;
        for (auto& it : m_task_infos) {
            ids.push_back(it.first);
        }

        return std::move(ids);
    }

    std::shared_ptr<TaskInfo> getTaskInfo(const std::string& task_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_task_infos.find(task_id);
        if (it != m_task_infos.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void add(const std::shared_ptr<TaskInfo>& taskinfo_ptr) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_infos[taskinfo_ptr->getTaskId()] = taskinfo_ptr;
        taskinfo_ptr->updateToDB(m_db);
    }

    void del(const std::string& task_id) {
        RwMutex::WriteLock wlock(m_mtx);
        auto it = m_task_infos.find(task_id);
        if (it != m_task_infos.end()) {
            it->second->deleteFromDB(m_db);
            m_task_infos.erase(it);
        }
    }

    void update(const std::shared_ptr<TaskInfo> &taskinfo_ptr) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_infos[taskinfo_ptr->getTaskId()] = taskinfo_ptr;
        taskinfo_ptr->updateToDB(m_db);
    }

    void update_running_tasks() {
		RwMutex::WriteLock wlock(m_running_mtx);
		m_running_tasks.clear();

        do {
            RwMutex::ReadLock rlock(m_mtx);
            for (auto& iter : m_task_infos) {
                if (iter.second->getTaskStatus() == TaskStatus::TS_Task_Running) {
                    m_running_tasks.push_back(iter.second->getTaskId());
                }
            }
        } while (0);

        m_running_db.update(m_running_tasks);
    }

    std::vector<std::string> getRunningTasks() {
        RwMutex::ReadLock rlock(m_running_mtx);
        return m_running_tasks;
    }

private:
    mutable RwMutex m_mtx;
    TaskInfoDB m_db;
    std::map<std::string, std::shared_ptr<TaskInfo> > m_task_infos;

	// running task
    mutable RwMutex m_running_mtx;
	RunningTaskDB m_running_db;
    std::vector<std::string> m_running_tasks;
};

typedef TaskInfoManager TaskInfoMgr;

#endif
