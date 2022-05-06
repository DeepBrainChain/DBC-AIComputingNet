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
