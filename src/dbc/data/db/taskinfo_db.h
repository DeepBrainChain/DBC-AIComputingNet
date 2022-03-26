#ifndef DBC_TASKINFO_DB_H
#define DBC_TASKINFO_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "../db_types/taskinfo_types.h"

namespace bfs = boost::filesystem;

class TaskInfoDB
{
public:
    TaskInfoDB() = default;

    virtual ~TaskInfoDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_tasks(std::map<std::string, std::shared_ptr<dbc::TaskInfo>>& tasks);

    bool delete_task(const std::string& task_id);

    std::shared_ptr<dbc::TaskInfo> read_task(const std::string& task_id);

    bool write_task(const std::shared_ptr<dbc::TaskInfo>& task);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

class WalletTaskDB
{
public:
    WalletTaskDB() = default;

    virtual ~WalletTaskDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_data(std::map<std::string, std::shared_ptr<dbc::rent_task>>& datas);

    bool delete_data(const std::string& wallet);

    std::shared_ptr<dbc::rent_task> read_data(const std::string& wallet);

    bool write_data(const std::shared_ptr<dbc::rent_task>& data);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif