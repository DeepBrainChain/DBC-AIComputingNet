#ifndef DBC_WALLET_RENT_TASK_H
#define DBC_WALLET_RENT_TASK_H

#include "util/utils.h"
#include "db/db_types/db_wallet_renttask_types.h"
#include "db/wallet_renttask_db.h"

class WalletRentTask {
public:
	friend class WalletRentTaskManager;

	WalletRentTask() {
		m_db_info = std::make_shared<dbc::db_wallet_renttask>();
	}

	virtual ~WalletRentTask() = default;

	std::string getRentWallet() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->rent_wallet;
	}

	void setRentWallet(const std::string& wallet) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_rent_wallet(wallet);
	}

	std::vector<std::string> getTaskIds() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->task_ids;
	}

	void setTaskIds(const std::vector<std::string>& ids) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_task_ids(ids);
	}

	void addTaskId(const std::string& id) {
		RwMutex::WriteLock wlock(m_mtx);
		std::vector<std::string> ids = m_db_info->task_ids;
		ids.push_back(id);
		m_db_info->__set_task_ids(ids);
	}

	int64_t getRentEnd() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->rent_end;
	}

	void setRentEnd(int64_t rentend) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_rent_end(rentend);
	}

	void deleteFromDB(WalletRentTaskDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.delete_data(m_db_info->rent_wallet);
	}

	void updateToDB(WalletRentTaskDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.write_data(m_db_info);
	}

private:
	mutable RwMutex m_mtx;
    std::shared_ptr<dbc::db_wallet_renttask> m_db_info;
};

#endif