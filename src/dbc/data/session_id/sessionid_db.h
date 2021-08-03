#ifndef DBC_SESSIONID_DB_HS
#define DBC_SESSIONID_DB_HS

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "sessionid_db_types.h"

class SessionIdDB
{
public:
    SessionIdDB() = default;

    virtual ~SessionIdDB() = default;

    bool init_db(boost::filesystem::path task_db_path, std::string db_name);

    void load(std::map<std::string, std::shared_ptr<dbc::owner_sessionid>>& session_ids);

    bool del(const std::string& wallet);

    std::shared_ptr<dbc::owner_sessionid> read(const std::string& wallet);

    bool write(const std::shared_ptr<dbc::owner_sessionid>& session_id);

protected:
    std::unique_ptr<leveldb::DB> m_db;
};

#endif