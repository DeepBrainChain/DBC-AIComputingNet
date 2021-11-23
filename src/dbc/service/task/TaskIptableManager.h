#ifndef DBC_TASKIPTABLE_MANAGER_H
#define DBC_TASKIPTABLE_MANAGER_H

#include "util/utils.h"
#include "db/iptable_db.h"

class TaskIptableManager : public Singleton<TaskIptableManager> {
public:
    TaskIptableManager() = default;

    virtual ~TaskIptableManager() = default;

    bool init();

    std::shared_ptr<dbc::task_iptable> getIptable(const std::string& task_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_task_iptables.find(task_id);
        if (it != m_task_iptables.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void delIptable(const std::string& task_id) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_iptables.erase(task_id);
        m_db.delete_iptable(task_id);
    }

    void addIptable(const std::shared_ptr<dbc::task_iptable>& iptable) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_iptables[iptable->task_id] = iptable;
        m_db.write_iptable(iptable);
    }

private:
    mutable RwMutex m_mtx;
    IpTableDB m_db;
    std::map<std::string, std::shared_ptr<dbc::task_iptable> > m_task_iptables;

};

typedef TaskIptableManager TaskIptableMgr;

#endif
