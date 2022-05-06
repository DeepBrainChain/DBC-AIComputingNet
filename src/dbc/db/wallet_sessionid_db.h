#ifndef DBC_WALLET_SESSIONID_DB_H
#define DBC_WALLET_SESSIONID_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/db_wallet_sessionid_types.h"

namespace bfs = boost::filesystem;

class WalletSessionIdDB
{
public:
    WalletSessionIdDB() = default;

    virtual ~WalletSessionIdDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_wallet_sessionid>>& session_ids);

    std::shared_ptr<dbc::db_wallet_sessionid> read_data(const std::string& wallet);

    bool write_data(const std::shared_ptr<dbc::db_wallet_sessionid>& session_id);

	bool delete_data(const std::string& wallet);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif