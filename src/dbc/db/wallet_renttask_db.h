#ifndef DBC_WALLET_RENTTASK_DB_H
#define DBC_WALLET_RENTTASK_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_wallet_renttask_types.h"

namespace bfs = boost::filesystem;

class WalletRentTaskDB
{
public:
    WalletRentTaskDB() = default;

    virtual ~WalletRentTaskDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_wallet_renttask>>& datas);

    std::shared_ptr<dbc::db_wallet_renttask> read_data(const std::string& wallet);

    bool write_data(const std::shared_ptr<dbc::db_wallet_renttask>& data);

	bool delete_data(const std::string& wallet);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif