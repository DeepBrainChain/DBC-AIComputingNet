#include "bare_metal_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

bool BareMetalDB::init_db(boost::filesystem::path db_path, std::string db_name)
{
    leveldb::DB* db = nullptr;
    leveldb::Options  options;
    options.create_if_missing = true;

    if (!bfs::exists(db_path))
    {
        LOG_INFO << "db directory path does not exist and create db directory" << db_path;
        bfs::create_directory(db_path);
    }

    if (!bfs::is_directory(db_path))
    {
        LOG_INFO << "db directory path does not exist and exit" << db_path;
        return false;
    }

    db_path /= bfs::path(db_name);

    leveldb::Status status = leveldb::DB::Open(options, db_path.generic_string(), &db);
    if (!status.ok())
    {
        LOG_ERROR << "open db error: " << status.ToString();
        return false;
    }

    m_db.reset(db);

    return true;
}

void BareMetalDB::load_datas(std::map<std::string, std::shared_ptr<dbc::db_bare_metal>>& bms)
{
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::db_bare_metal> bm = std::make_shared<dbc::db_bare_metal>();
        std::shared_ptr<byte_buf> bm_buf = std::make_shared<byte_buf>();
        bm_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        network::binary_protocol proto(bm_buf.get());
        bm->read(&proto);

        bms.insert({ bm->node_id, bm });
    }
}

std::shared_ptr<dbc::db_bare_metal> BareMetalDB::read_data(const std::string& node_id) {
    if (node_id.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), node_id, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::db_bare_metal> bm = std::make_shared<dbc::db_bare_metal>();
        std::shared_ptr<byte_buf> bm_buf = std::make_shared<byte_buf>();
        bm_buf->write_to_byte_buf(val.c_str(), val.size());
        network::binary_protocol proto(bm_buf.get());
        bm->read(&proto);
        return bm;
    } else {
        return nullptr;
    }
}

bool BareMetalDB::write_data(const std::shared_ptr<dbc::db_bare_metal>& bm)
{
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    bm->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, bm->node_id, slice);
    if (status.ok()) {
        return true;
    } else {
        LOG_ERROR << "put data failed: " << bm->node_id;
        return false;
    }
}

bool BareMetalDB::delete_data(const std::string& node_id)
{
	if (node_id.empty())
		return true;

	leveldb::WriteOptions write_options;
	write_options.sync = true;
	leveldb::Status status = m_db->Delete(write_options, node_id);
	if (status.ok()) {
		return true;
	} else {
		return false;
	}
}
