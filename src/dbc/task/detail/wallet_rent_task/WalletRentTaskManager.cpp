#include "WalletRentTaskManager.h"
#include "log/log.h"

FResult WalletRentTaskManager::init() {
    bool ret = m_db.init_db(EnvManager::instance().get_db_path(), "rent_task.db");
    if (!ret) {
        return FResult(ERR_ERROR, "init wallet_task_db failed");
    }

    std::map<std::string, std::shared_ptr<dbc::db_wallet_renttask>> mp_renttasks;
    m_db.load_datas(mp_renttasks);
    
    for (auto& iter : mp_renttasks) {
        std::shared_ptr<WalletRentTask> ptr = std::make_shared<WalletRentTask>();
        ptr->m_db_info = iter.second;
        m_wallet_renttasks.insert({ iter.first, ptr });
    }

    return FResultOk;
}

void WalletRentTaskManager::del(const std::string &task_id) {
    RwMutex::WriteLock wlock(m_mtx);
    for (auto& it : m_wallet_renttasks) {
        std::shared_ptr<WalletRentTask> renttask = it.second;
        std::vector<std::string> vec = renttask->getTaskIds();
        bool found = false;
        for (auto iter = vec.begin(); iter != vec.end(); ) {
            if (*iter == task_id) {
                iter = vec.erase(iter);
                found = true;
                break;
            } else {
                ++iter;
            }
        }

        if (found) {
            renttask->setTaskIds(vec);
            renttask->updateToDB(m_db);
            break;
        }
    }
}
