#include "WalletSessionIDManager.h"
#include "log/log.h"

FResult WalletSessionIDManager::init() {
    bool ret = m_db.init_db(EnvManager::instance().get_db_path(), "sessionid.db");
    if (!ret) {
        return FResult(ERR_ERROR, "init sessionid_db failed");
    }

    std::map<std::string, std::shared_ptr<dbc::db_wallet_sessionid>> mp_walletsessionids;
    m_db.load_datas(mp_walletsessionids);

    for (auto &iter: mp_walletsessionids) {
        std::shared_ptr<WalletSessionId> ptr = std::make_shared<WalletSessionId>();
        ptr->m_db_info = iter.second;
        m_wallet_sessionid.insert({iter.first, ptr});
        m_sessionid_wallet.insert({iter.second->session_id, ptr});
    }

    return FResultOk;
}

std::map<std::string, std::shared_ptr<WalletSessionId>> WalletSessionIDManager::getAllWalletSessionIds() {
    RwMutex::ReadLock rlock(m_mtx);
    return m_wallet_sessionid;
}

std::string WalletSessionIDManager::getSessionIdByWallet(const std::string& wallet) const {
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_wallet_sessionid.find(wallet);
    if (it != m_wallet_sessionid.end()) {
        return it->second->getSessionId();
    } else {
        return "";
    }
}

std::shared_ptr<WalletSessionId> WalletSessionIDManager::getWalletSessionIdBySessionId(const std::string& session_id) const {
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_sessionid_wallet.find(session_id);
    if (it != m_sessionid_wallet.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

void WalletSessionIDManager::add(const std::shared_ptr<WalletSessionId>& wallet_sessionid) {
    RwMutex::WriteLock wlock(m_mtx);
    m_wallet_sessionid[wallet_sessionid->getRentWallet()] = wallet_sessionid;
    m_sessionid_wallet[wallet_sessionid->getSessionId()] = wallet_sessionid;
    wallet_sessionid->updateToDB(m_db);
}

void WalletSessionIDManager::del(const std::string& wallet) {
    RwMutex::WriteLock wlock(m_mtx);
    auto iter = m_wallet_sessionid.find(wallet);
    if (iter != m_wallet_sessionid.end()) {
        std::string session_id = iter->second->getSessionId();
        m_sessionid_wallet.erase(session_id);
        m_wallet_sessionid.erase(iter);
    }
}

void WalletSessionIDManager::update(const std::shared_ptr<WalletSessionId>& wallet_sessionid) {
    RwMutex::WriteLock wlock(m_mtx);
    m_wallet_sessionid[wallet_sessionid->getRentWallet()] = wallet_sessionid;
    m_sessionid_wallet[wallet_sessionid->getSessionId()] = wallet_sessionid;
    wallet_sessionid->updateToDB(m_db);
}

std::string WalletSessionIDManager::createSessionId(const std::string &wallet, const std::vector<std::string>& multisig_signers) {
    {
        RwMutex::ReadLock rlock(m_mtx);
        auto it = m_wallet_sessionid.find(wallet);
        if (it != m_wallet_sessionid.end()) {
            return it->second->getSessionId();
        }
    }

    std::string session_id = util::create_session_id();
    std::shared_ptr<WalletSessionId> ptr = std::make_shared<WalletSessionId>();
    ptr->setRentWallet(wallet);
    ptr->setSessionId(session_id);
    ptr->setMultisigSigners(multisig_signers);
    add(ptr);
    return session_id;
}

std::string WalletSessionIDManager::checkSessionId(const std::string& session_id, const std::string& session_id_sign) {
    if (session_id.empty() || session_id_sign.empty())
        return "";
    
    RwMutex::ReadLock rlock(m_mtx);
    auto it = m_sessionid_wallet.find(session_id);
    if (it == m_sessionid_wallet.end()) {
        return "";
    }

    if (util::verify_sign(session_id_sign, session_id, it->second->getRentWallet())) {
        return it->second->getRentWallet();
    }

    auto multisig_signers = it->second->getMultisigSigners();
    for (auto& it_wallet : multisig_signers) {
        if (util::verify_sign(session_id_sign, session_id, it_wallet)) {
            return it->second->getRentWallet();
        }
    }

    return "";
}
