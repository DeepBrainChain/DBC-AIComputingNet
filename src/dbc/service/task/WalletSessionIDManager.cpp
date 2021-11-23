#include "WalletSessionIDManager.h"
#include "log/log.h"

bool WalletSessionIDManager::init() {
    bool ret = m_db.init_db(env_manager::instance().get_db_path(), "sessionid.db");
    if (!ret) {
        LOG_ERROR << "init sessionid_db failed";
        return false;
    }

    std::map<std::string, std::shared_ptr<dbc::rent_sessionid>> wallet_sessionid;
    m_db.load(wallet_sessionid);
    for (auto &it: wallet_sessionid) {
        m_wallet_sessionid.insert({it.first, it.second});
        m_sessionid_wallet.insert({it.second->session_id, it.second});
    }

    return true;
}
