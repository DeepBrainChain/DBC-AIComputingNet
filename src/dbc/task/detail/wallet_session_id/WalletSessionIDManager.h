#ifndef DBC_WALLET_SESSIONID_MANAGER_H
#define DBC_WALLET_SESSIONID_MANAGER_H

#include "util/utils.h"
#include "db/wallet_sessionid_db.h"
#include "WalletSessionId.h"

class WalletSessionIDManager : public Singleton<WalletSessionIDManager> {
public:
    friend class TaskManager;

    WalletSessionIDManager() = default;

    virtual ~WalletSessionIDManager() = default;

    FResult init();

    std::map<std::string, std::shared_ptr<WalletSessionId>> getAllWalletSessionIds() {
        RwMutex::ReadLock rlock(m_mtx);
        return m_wallet_sessionid;
    }

    std::shared_ptr<WalletSessionId> geWalletSessionIdByWallet(const std::string& wallet) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_wallet_sessionid.find(wallet);
        if (it != m_wallet_sessionid.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<WalletSessionId> getWalletSessionIdBySessionId(const std::string& session_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_sessionid_wallet.find(session_id);
        if (it != m_sessionid_wallet.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void add(const std::shared_ptr<WalletSessionId>& wallet_sessionid) {
        RwMutex::WriteLock wlock(m_mtx);
        m_wallet_sessionid[wallet_sessionid->getRentWallet()] = wallet_sessionid;
        m_sessionid_wallet[wallet_sessionid->getSessionId()] = wallet_sessionid;
        wallet_sessionid->updateToDB(m_db);
    }

    void del(const std::string& wallet) {
        RwMutex::WriteLock wlock(m_mtx);
        auto iter = m_wallet_sessionid.find(wallet);
        if (iter != m_wallet_sessionid.end()) {
            std::string session_id = iter->second->getSessionId();
            m_sessionid_wallet.erase(session_id);
            m_wallet_sessionid.erase(iter);
        }
    }

	void update(const std::shared_ptr<WalletSessionId>& wallet_sessionid) {
		RwMutex::WriteLock wlock(m_mtx);
		m_wallet_sessionid[wallet_sessionid->getRentWallet()] = wallet_sessionid;
		m_sessionid_wallet[wallet_sessionid->getSessionId()] = wallet_sessionid;
		wallet_sessionid->updateToDB(m_db);
	}

private:
    mutable RwMutex m_mtx;
    WalletSessionIdDB m_db;
    std::map<std::string, std::shared_ptr<WalletSessionId>> m_wallet_sessionid;
    std::map<std::string, std::shared_ptr<WalletSessionId>> m_sessionid_wallet;
};

typedef WalletSessionIDManager WalletSessionIDMgr;

#endif
