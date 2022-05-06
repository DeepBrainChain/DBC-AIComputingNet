#ifndef DBC_TASKIPTABLE_MANAGER_H
#define DBC_TASKIPTABLE_MANAGER_H

#include "util/utils.h"
#include "db/task_iptable_db.h"
#include "IptableInfo.h"

class TaskIptableManager : public Singleton<TaskIptableManager> {
public:
    friend class TaskManager;

    TaskIptableManager() = default;

    virtual ~TaskIptableManager() = default;

    FResult init();

    std::shared_ptr<IptableInfo> getIptableInfo(const std::string& task_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_task_iptables.find(task_id);
        if (it != m_task_iptables.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void add(const std::shared_ptr<IptableInfo>& iptableinfo_ptr) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_iptables[iptableinfo_ptr->getTaskId()] = iptableinfo_ptr;
        iptableinfo_ptr->updateToDB(m_db);
    }

	void del(const std::string& task_id) {
		RwMutex::WriteLock wlock(m_mtx);
		auto it = m_task_iptables.find(task_id);
		if (it != m_task_iptables.end()) {
			it->second->deleteFromDB(m_db);
			m_task_iptables.erase(it);
		}
	}

	void update(const std::shared_ptr<IptableInfo>& iptableinfo_ptr) {
		RwMutex::WriteLock wlock(m_mtx);
		m_task_iptables[iptableinfo_ptr->getTaskId()] = iptableinfo_ptr;
        iptableinfo_ptr->updateToDB(m_db);
	}

private:
    mutable RwMutex m_mtx;
    TaskIptableDB m_db;
    std::map<std::string, std::shared_ptr<IptableInfo> > m_task_iptables;
};

typedef TaskIptableManager TaskIptableMgr;

#endif
