#include "TaskInfoManager.h"
#include "log/log.h"

bool TaskInfoManager::init() {
    bool ret = m_db.init_db(EnvManager::instance().get_db_path(), "task.db");
    if (!ret) {
        LOG_ERROR << "init task_db failed";
        return false;
    }

    m_db.load_tasks(m_tasks);
    return true;
}

