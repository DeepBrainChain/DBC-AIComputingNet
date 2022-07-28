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
        RwMutex::WriteLock wlock2(m_deleted_mtx);
        std::shared_ptr<dbc::db_task_info> info = it->second->m_db_info;
        info->__set_delete_time(time(NULL));
        m_deleted_tasks[task_id] = info;
        m_deleted_db.write_data(info);
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

    void update_deleted_tasks() {
        int64_t cur_time = time(NULL);
        RwMutex::WriteLock wlock(m_deleted_mtx);
        for (auto iter = m_deleted_tasks.begin(); iter != m_deleted_tasks.end();) {
            if (difftime(cur_time, iter->second->delete_time) > 3600 * 24 * 30 * 3) {
                m_deleted_db.delete_data(iter->first);
                iter = m_deleted_tasks.erase(iter);
            } else {
                iter++;
            }
        }
    }

    std::shared_ptr<dbc::db_task_info> get_deleted_task(const std::string& task_id) {
        RwMutex::ReadLock rlock(m_deleted_mtx);
        auto iter = m_deleted_tasks.find(task_id);
        if (iter != m_deleted_tasks.end()) return iter->second;
        return nullptr;
    }

private:
    mutable RwMutex m_mtx;
    TaskInfoDB m_db;
    std::map<std::string, std::shared_ptr<TaskInfo> > m_task_infos;

	// running task
    mutable RwMutex m_running_mtx;
	RunningTaskDB m_running_db;
    std::vector<std::string> m_running_tasks;

    mutable RwMutex m_deleted_mtx;
    TaskInfoDB m_deleted_db;
    std::map<std::string, std::shared_ptr<dbc::db_task_info>> m_deleted_tasks;
};

typedef TaskInfoManager TaskInfoMgr;

#endif
