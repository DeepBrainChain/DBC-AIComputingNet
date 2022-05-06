#ifndef DBC_TASK_SNAPSHOT_DB_H
#define DBC_TASK_SNAPSHOT_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_task_snapshot_types.h"

namespace bfs = boost::filesystem;

class TaskSnapshotDB
{
public:
    TaskSnapshotDB() = default;

    virtual ~TaskSnapshotDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_snapshot_info>>& datas);

    std::shared_ptr<dbc::db_snapshot_info> read_data(const std::string& task_id);

    bool write_data(const std::shared_ptr<dbc::db_snapshot_info>& data);

	bool delete_data(const std::string& task_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif