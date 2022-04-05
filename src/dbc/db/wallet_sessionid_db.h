#ifndef DBC_WALLET_SESSIONID_DB_H
#define DBC_WALLET_SESSIONID_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "db_types/wallet_sessionid_types.h"

namespace bfs = boost::filesystem;

class WalletSessionIdDB
{
public:
    WalletSessionIdDB() = default;

    virtual ~WalletSessionIdDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load(std::map<std::string, std::shared_ptr<dbc::rent_sessionid>>& session_ids);

    bool del(const std::string& wallet);

    std::shared_ptr<dbc::rent_sessionid> read(const std::string& wallet);

    bool write(const std::shared_ptr<dbc::rent_sessionid>& session_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif