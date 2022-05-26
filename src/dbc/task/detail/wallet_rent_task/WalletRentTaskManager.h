#ifndef DBC_WALLETRENTTASK_MANAGER_H
#define DBC_WALLETRENTTASK_MANAGER_H

#include "util/utils.h"
#include "db/wallet_renttask_db.h"
#include "WalletRentTask.h"

class WalletRentTaskManager : public Singleton<WalletRentTaskManager> {
public:
    friend class TaskManager;

    WalletRentTaskManager() = default;

    virtual ~WalletRentTaskManager() = default;

    FResult init();

    std::map<std::string, std::shared_ptr<WalletRentTask> > getAllWalletRentTasks() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_wallet_renttasks;
    }

    std::shared_ptr<WalletRentTask> getWalletRentTask(const std::string& wallet) {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_wallet_renttasks.find(wallet);
        if (it != m_wallet_renttasks.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

	std::vector<std::string> getAllWallets() const {
		RwMutex::ReadLock rlock(m_mtx);
		std::vector<std::string> wallets;
		for (auto& it : m_wallet_renttasks) {
			wallets.push_back(it.first);
		}
		return wallets;
	}

	void add(const std::string& wallet, const std::string& task_id, int64_t rent_end) {
		RwMutex::WriteLock wlock(m_mtx);
		auto it = m_wallet_renttasks.find(wallet);
		if (it == m_wallet_renttasks.end()) {
			std::shared_ptr<WalletRentTask> renttask = std::make_shared<WalletRentTask>();
			renttask->setRentWallet(wallet);
			renttask->setRentEnd(rent_end);
			renttask->addTaskId(task_id);
			m_wallet_renttasks[wallet] = renttask;
			renttask->updateToDB(m_db);
		}
		else {
			it->second->addTaskId(task_id);
			it->second->setRentEnd(rent_end);
			it->second->updateToDB(m_db);
		}
	}

	void add(const std::shared_ptr<WalletRentTask>& renttask) {
		RwMutex::WriteLock wlock(m_mtx);
		m_wallet_renttasks[renttask->getRentWallet()] = renttask;
		renttask->updateToDB(m_db);
	}

	void del(const std::string& task_id);

    void update(const std::shared_ptr<WalletRentTask>& renttask) {
        RwMutex::WriteLock wlock(m_mtx);
        m_wallet_renttasks[renttask->getRentWallet()] = renttask;
        renttask->updateToDB(m_db);
    }

    void update(const std::map<std::string, std::shared_ptr<WalletRentTask> >& renttasks) {
		RwMutex::WriteLock wlock(m_mtx);
		m_wallet_renttasks = renttasks;
		for (auto& it : m_wallet_renttasks) {
			it.second->updateToDB(m_db);
		}
    }

private:
    mutable RwMutex m_mtx;
    WalletRentTaskDB m_db;
    std::map<std::string, std::shared_ptr<WalletRentTask> > m_wallet_renttasks;
};

typedef WalletRentTaskManager WalletRentTaskMgr;

#endif
