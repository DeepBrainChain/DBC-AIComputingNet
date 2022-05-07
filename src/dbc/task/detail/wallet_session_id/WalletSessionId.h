#ifndef DBC_WALLET_SESSIONID_H
#define DBC_WALLET_SESSIONID_H

#include "util/utils.h"
#include "db/db_types/db_wallet_sessionid_types.h"
#include "db/wallet_renttask_db.h"

class WalletSessionId {
public:
	friend class WalletSessionIDManager;

	WalletSessionId() {
		m_db_info = std::make_shared<dbc::db_wallet_sessionid>();
	}

	virtual ~WalletSessionId() = default;

	std::string getRentWallet() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->rent_wallet;
	}

	void setRentWallet(const std::string& wallet) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_rent_wallet(wallet);
	}

	std::string getSessionId() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->session_id;
	}

	void setSessionId(const std::string& id) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_session_id(id);
	}

	std::vector<std::string> getMultisigSigners() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->multisig_signers;
	}

	void setMultisigSigners(const std::vector<std::string>& signers) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_multisig_signers(signers);
	}

	void updateToDB(WalletSessionIdDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.write_data(m_db_info);
	}

	void deleteFromDB(WalletSessionIdDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.delete_data(m_db_info->rent_wallet);
	}

private:
	mutable RwMutex m_mtx;
    std::shared_ptr<dbc::db_wallet_sessionid> m_db_info;
};

#endif