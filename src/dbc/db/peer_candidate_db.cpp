#include "peer_candidate_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

bool PeerCandidateDB::init(const bfs::path& db_file_path, const std::string& db_file_name)
{
    if (!bfs::exists(db_file_path)) {
        bfs::create_directory(db_file_path);
    }

    if (!bfs::is_directory(db_file_path)) {
        LOG_ERROR << "db_file_path not exist: " << db_file_path;
        return false;
    }

    bfs::path db_file_full_path = db_file_path / bfs::path(db_file_name);

    leveldb::DB* db = nullptr;
    leveldb::Options  options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, db_file_full_path.generic_string(), &db);
    if (!status.ok()) {
        LOG_ERROR << "open db error: db=" << db_file_full_path.generic_string() << ", status=" << status.ToString();
        return false;
    }

    m_db.reset(db);

    return true;
}

void PeerCandidateDB::load_datas(std::map<std::string, std::shared_ptr<dbc::db_peer_candidate>>& datas)
{
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::db_peer_candidate> item = std::make_shared<dbc::db_peer_candidate>();
        std::shared_ptr<byte_buf> b_buf = std::make_shared<byte_buf>();
        b_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        network::binary_protocol proto(b_buf.get());
        item->read(&proto);

        datas.insert({item->ip, item});
    }
}

std::shared_ptr<dbc::db_peer_candidate> PeerCandidateDB::read(const std::string& ip) {
    if (ip.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), ip, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::db_peer_candidate> item = std::make_shared<dbc::db_peer_candidate>();
        std::shared_ptr<byte_buf> b_buf = std::make_shared<byte_buf>();
        b_buf->write_to_byte_buf(val.c_str(), val.size());
        network::binary_protocol proto(b_buf.get());
        item->read(&proto);

        return item;
    }
    else {
        return nullptr;
    }
}

bool PeerCandidateDB::write(const std::shared_ptr<dbc::db_peer_candidate>& item)
{
    std::shared_ptr<byte_buf> b_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(b_buf.get());
    item->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(b_buf->get_read_ptr(), b_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, item->ip, slice);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_ERROR << "write peer_candidate failed: " 
            << "node_type=" << item->node_type << ", node_id=" << item->node_id 
            << ", addr=" << item->ip << ":" << item->port;
        return false;
    }
}

bool PeerCandidateDB::write(const std::vector<std::shared_ptr<dbc::db_peer_candidate>>& items) {
    leveldb::WriteBatch batch;
    for (auto& it : items) {
        std::shared_ptr<byte_buf> b_buf = std::make_shared<byte_buf>();
        network::binary_protocol proto(b_buf.get());
        it->write(&proto);

        leveldb::Slice slice(b_buf->get_read_ptr(), b_buf->get_valid_read_len());
        batch.Put(it->ip, slice);
    }

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = m_db->Write(write_options, &batch);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_ERROR << "write peer_candidate failed: batch_size=" << items.size();
        return false;
    }
}

bool PeerCandidateDB::write(const std::list<std::shared_ptr<dbc::db_peer_candidate>>& items) {
    leveldb::WriteBatch batch;
    for (auto& it : items) {
        std::shared_ptr<byte_buf> b_buf = std::make_shared<byte_buf>();
        network::binary_protocol proto(b_buf.get());
        it->write(&proto);

        leveldb::Slice slice(b_buf->get_read_ptr(), b_buf->get_valid_read_len());
        batch.Put(it->ip, slice);
    }

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = m_db->Write(write_options, &batch);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_ERROR << "write peer_candidate failed: batch_size=" << items.size();
        return false;
    }
}

bool PeerCandidateDB::del(const std::string& ip)
{
    if (ip.empty())
        return true;

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Status status = m_db->Delete(write_options, ip);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_ERROR << "delete peer_candidate failed: key=" << ip;
        return false;
    }
}

void PeerCandidateDB::clear() {
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        m_db->Delete(write_options, it->key());
    }
}
