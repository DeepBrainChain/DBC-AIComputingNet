#ifndef DBCPROJ_TASKIPTABLE_DB_H
#define DBCPROJ_TASKIPTABLE_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "task_iptable_types.h"

class TaskIpTableDB {
public:
    TaskIpTableDB() = default;

    virtual ~TaskIpTableDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_iptables(std::map<std::string, std::shared_ptr<dbc::TaskIpTable>>& task_iptables);

    bool delete_iptable(const std::string& task_id);

    std::shared_ptr<dbc::TaskIpTable> read_iptable(const std::string& task_id);

    bool write_iptable(const std::shared_ptr<dbc::TaskIpTable>& task_iptable);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif //DBCPROJ_TASKIPTABLE_DB_H
