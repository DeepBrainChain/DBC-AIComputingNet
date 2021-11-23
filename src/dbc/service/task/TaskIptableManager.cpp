#include "TaskIptableManager.h"
#include "log/log.h"

bool TaskIptableManager::init() {
    bool ret = m_db.init_db(env_manager::instance().get_db_path(), "iptable.db");
    if (!ret) {
        LOG_ERROR << "init iptable_db failed";
        return false;
    }

    m_db.load_iptables(m_task_iptables);
    return true;
}
