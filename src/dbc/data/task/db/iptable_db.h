#ifndef DBCPROJ_IPTABLE_DB_H
#define DBCPROJ_IPTABLE_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "iptable_db_types.h"

class IpTableDB {
public:
    IpTableDB() = default;

    virtual ~IpTableDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_iptables(std::map<std::string, std::shared_ptr<dbc::task_iptable>>& task_iptables);

    bool delete_iptable(const std::string& task_id);

    std::shared_ptr<dbc::task_iptable> read_iptable(const std::string& task_id);

    bool write_iptable(const std::shared_ptr<dbc::task_iptable>& task_iptable);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif //DBCPROJ_IPTABLE_DB_H
