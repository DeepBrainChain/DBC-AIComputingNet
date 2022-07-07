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

    std::map<std::string, std::shared_ptr<WalletSessionId>> getAllWalletSessionIds();

    std::string getSessionIdByWallet(const std::string& wallet) const;

    std::shared_ptr<WalletSessionId> getWalletSessionIdBySessionId(const std::string& session_id) const;

    void add(const std::shared_ptr<WalletSessionId>& wallet_sessionid);

    void del(const std::string& wallet);

	void update(const std::shared_ptr<WalletSessionId>& wallet_sessionid);

    std::string createSessionId(const std::string &wallet,
        const std::vector<std::string>& multisig_signers = std::vector<std::string>());

    std::string checkSessionId(const std::string& session_id, const std::string& session_id_sign);

private:
    mutable RwMutex m_mtx;
    WalletSessionIdDB m_db;
    std::map<std::string, std::shared_ptr<WalletSessionId>> m_wallet_sessionid;
    std::map<std::string, std::shared_ptr<WalletSessionId>> m_sessionid_wallet;
};

typedef WalletSessionIDManager WalletSessionIDMgr;

#endif
