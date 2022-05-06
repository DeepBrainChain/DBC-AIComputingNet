#include "TaskIptableManager.h"
#include "log/log.h"

FResult TaskIptableManager::init() {
    bool ret = m_db.init_db(EnvManager::instance().get_db_path(), "iptable.db");
    if (!ret) {
        return FResult(ERR_ERROR, "init iptable_db failed");
    }

    std::map<std::string, std::shared_ptr<dbc::db_task_iptable>> mp_iptables;
    m_db.load_datas(mp_iptables);
    
    for (auto& iter : mp_iptables) {
        std::shared_ptr<IptableInfo> ptr = std::make_shared<IptableInfo>();
        ptr->m_db_info = iter.second;
        m_task_iptables.insert({ iter.first, ptr });
    }

    return FResultOk;
}
