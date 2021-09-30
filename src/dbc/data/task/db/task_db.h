#ifndef DBC_TASK_DB_HS
#define DBC_TASK_DB_HS

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "task_db_types.h"

class TaskDB
{
public:
    TaskDB() = default;

    virtual ~TaskDB() = default;

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