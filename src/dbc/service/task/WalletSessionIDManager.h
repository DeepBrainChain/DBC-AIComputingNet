#ifndef DBC_WALLETSESSIONID_MANAGER_H
#define DBC_WALLETSESSIONID_MANAGER_H

#include "util/utils.h"
#include "db/sessionid_db.h"

class WalletSessionIDManager : public Singleton<WalletSessionIDManager> {
public:
    WalletSessionIDManager() = default;

    virtual ~WalletSessionIDManager() = default;

    bool init();

    std::map<std::string, std::shared_ptr<dbc::rent_sessionid>> getAllWalletSessions() {
        RwMutex::ReadLock rlock(m_mtx);
        return m_wallet_sessionid;
    }

    std::shared_ptr<dbc::rent_sessionid> getSessionIdByWallet(const std::string& wallet) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_wallet_sessionid.find(wallet);
        if (it != m_wallet_sessionid.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<dbc::rent_sessionid> findSessionId(const std::string& session_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_sessionid_wallet.find(session_id);
        if (it != m_sessionid_wallet.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void addWalletSessionId(std::shared_ptr<dbc::rent_sessionid> wallet_sessionid) {
        RwMutex::WriteLock wlock(m_mtx);
        m_wallet_sessionid[wallet_sessionid->rent_wallet] = wallet_sessionid;
        m_sessionid_wallet[wallet_sessionid->session_id] = wallet_sessionid;
        m_db.write(wallet_sessionid);
    }

private:
    mutable RwMutex m_mtx;
    SessionIdDB m_db;
    std::map<std::string, std::shared_ptr<dbc::rent_sessionid>> m_wallet_sessionid;
    std::map<std::string, std::shared_ptr<dbc::rent_sessionid>> m_sessionid_wallet;
};

typedef WalletSessionIDManager WalletSessionIDMgr;

#endif
