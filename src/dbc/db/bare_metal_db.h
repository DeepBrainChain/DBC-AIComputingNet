
#ifndef DBC_BARE_METAL_DB_H
#define DBC_BARE_METAL_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_bare_metal_types.h"

namespace bfs = boost::filesystem;

class BareMetalDB
{
public:
    BareMetalDB() = default;

    virtual ~BareMetalDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>& bms);

    std::shared_ptr<dbc::db_bare_metal> read_data(const std::string& node_id);

    bool write_data(const std::shared_ptr<dbc::db_bare_metal>& bm);

	bool delete_data(const std::string& node_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif