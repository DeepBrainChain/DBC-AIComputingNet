#include "WalletRentTaskManager.h"
#include "log/log.h"

bool WalletRentTaskManager::init() {
    bool ret = m_db.init_db(env_manager::instance().get_db_path(), "rent_task.db");
    if (!ret) {
        LOG_ERROR << "init wallet_task_db failed";
        return false;
    }

    m_db.load_data(m_wallet_renttasks);
    return true;
}

void WalletRentTaskManager::update(const std::map<std::string, std::shared_ptr<dbc::rent_task>> &renttasks) {
    RwMutex::WriteLock wlock(m_mtx);
    m_wallet_renttasks = renttasks;
    for (auto& it : m_wallet_renttasks) {
        m_db.write_data(it.second);
    }
}

void WalletRentTaskManager::delRentTask(const std::string &task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    for (auto& it : m_wallet_renttasks) {
        std::shared_ptr<dbc::rent_task> renttask = it.second;
        std::vector<std::string> &vec = renttask->task_ids;
        bool bwrite = false;
        for (auto iter = vec.begin(); iter != vec.end(); ) {
            if (*iter == task_id) {
                iter = vec.erase(iter);
                bwrite = true;
                break;
            } else {
                ++iter;
            }
        }

        if (bwrite) {
            m_db.write_data(renttask);
            break;
        }
    }
}