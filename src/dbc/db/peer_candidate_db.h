#ifndef DBC_PEER_CANDIDATE_DB_H
#define DBC_PEER_CANDIDATE_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <boost/filesystem.hpp>
#include "db_types/peer_candidate_types.h"

namespace bfs = boost::filesystem;

class PeerCandidateDB
{
public:
    PeerCandidateDB() = default;

    virtual ~PeerCandidateDB() = default;

    bool init(const bfs::path& db_file_path, const std::string& db_file_name);

    void load_datas(std::map<std::string, std::shared_ptr<dbc::db_peer_candidate>>& datas);

    std::shared_ptr<dbc::db_peer_candidate> read(const std::string& ip);

    bool write(const std::shared_ptr<dbc::db_peer_candidate>& item);

    bool write(const std::vector<std::shared_ptr<dbc::db_peer_candidate>>& items);

    bool write(const std::list<std::shared_ptr<dbc::db_peer_candidate>>& items);

    bool del(const std::string& ip);

    void clear();

protected:
    std::unique_ptr<leveldb::DB> m_db; // <ip, db_peer_candidate>
};

#endif