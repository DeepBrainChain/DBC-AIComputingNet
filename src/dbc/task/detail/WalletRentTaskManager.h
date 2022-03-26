#ifndef DBC_WALLETRENTTASK_MANAGER_H
#define DBC_WALLETRENTTASK_MANAGER_H

#include "util/utils.h"
#include "data/db/taskinfo_db.h"

class WalletRentTaskManager : public Singleton<WalletRentTaskManager> {
public:
    WalletRentTaskManager() = default;

    virtual ~WalletRentTaskManager() = default;

    bool init();

    std::map<std::string, std::shared_ptr<dbc::rent_task> > getRentTasks() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_wallet_renttasks;
    }

    std::shared_ptr<dbc::rent_task> getRentTask(const std::string& wallet) {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_wallet_renttasks.find(wallet);
        if (it != m_wallet_renttasks.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void update(const std::shared_ptr<dbc::rent_task>& renttask) {
        RwMutex::WriteLock wlock(m_mtx);
        m_wallet_renttasks[renttask->rent_wallet] = renttask;
        m_db.write_data(renttask);
    }

    void update(const std::map<std::string, std::shared_ptr<dbc::rent_task> >& renttasks);

    std::vector<std::string> getAllWallet() const {
        RwMutex::ReadLock rlock(m_mtx);
        std::vector<std::string> wallets;
        for (auto & it : m_wallet_renttasks) {
            wallets.push_back(it.first);
        }
        return wallets;
    }

    void delRentTask(const std::string& task_id);

    void addRentTask(const std::string& wallet, const std::string& task_id, int64_t rent_end) {
        RwMutex::WriteLock wlock(m_mtx);
        auto it = m_wallet_renttasks.find(wallet);
        if (it == m_wallet_renttasks.end()) {
            std::shared_ptr<dbc::rent_task> renttask = std::make_shared<dbc::rent_task>();
            renttask->rent_wallet = wallet;
            renttask->rent_end = rent_end;
            renttask->task_ids.push_back(task_id);
            m_wallet_renttasks[wallet] = renttask;
            m_db.write_data(renttask);
        } else {
            it->second->task_ids.push_back(task_id);
            it->second->rent_end = rent_end;
            m_db.write_data(it->second);
        }
    }

    void addRentTask(const std::shared_ptr<dbc::rent_task>& renttask) {
        RwMutex::WriteLock wlock(m_mtx);
        m_wallet_renttasks[renttask->rent_wallet] = renttask;
        m_db.write_data(renttask);
    }

    void updateRentEnd(const std::string& wallet, int64_t rent_end) {
        RwMutex::WriteLock wlock(m_mtx);
        auto it = m_wallet_renttasks.find(wallet);
        if (it != m_wallet_renttasks.end()) {
            it->second->rent_end = rent_end;
            m_db.write_data(it->second);
        }
    }

private:
    mutable RwMutex m_mtx;
    WalletTaskDB m_db;
    std::map<std::string, std::shared_ptr<dbc::rent_task> > m_wallet_renttasks;
};

typedef WalletRentTaskManager WalletRentTaskMgr;

#endif
