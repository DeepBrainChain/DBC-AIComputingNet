#include "wallet_sessionid_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

bool WalletSessionIdDB::init_db(boost::filesystem::path task_db_path, std::string db_name)
{
    leveldb::DB* db = nullptr;
    leveldb::Options  options;
    options.create_if_missing = true;

    if (!bfs::exists(task_db_path))
    {
        LOG_INFO << "db directory path does not exist and create db directory" << task_db_path;
        bfs::create_directory(task_db_path);
    }

    if (!bfs::is_directory(task_db_path))
    {
        LOG_INFO << "db directory path does not exist and exit" << task_db_path;
        return false;
    }

    task_db_path /= bfs::path(db_name);

    leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
    if (!status.ok())
    {
        LOG_ERROR << "open db error: " << status.ToString();
        return false;
    }

    m_db.reset(db);

    return true;
}

void WalletSessionIdDB::load_datas(std::map<std::string, std::shared_ptr<dbc::db_wallet_sessionid>>& session_ids)
{
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::db_wallet_sessionid> sessionid = std::make_shared<dbc::db_wallet_sessionid>();
        std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
        buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        network::binary_protocol proto(buf.get());
        sessionid->read(&proto);

        session_ids.insert({ sessionid->rent_wallet, sessionid });
    }
}

std::shared_ptr<dbc::db_wallet_sessionid> WalletSessionIdDB::read_data(const std::string& wallet) {
    if (wallet.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), wallet, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::db_wallet_sessionid> sessionid = std::make_shared<dbc::db_wallet_sessionid>();
        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(val.c_str(), val.size());
        network::binary_protocol proto(task_buf.get());
        sessionid->read(&proto);
        return sessionid;
    }
    else {
        return nullptr;
    }
}

bool WalletSessionIdDB::write_data(const std::shared_ptr<dbc::db_wallet_sessionid>& session_id)
{
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    session_id->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, session_id->rent_wallet, slice);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_INFO << "db put data failed: " << session_id->rent_wallet;
        return false;
    }
}

bool WalletSessionIdDB::delete_data(const std::string& wallet)
{
	if (wallet.empty())
		return true;

	leveldb::WriteOptions write_options;
	write_options.sync = true;
	leveldb::Status status = m_db->Delete(write_options, wallet);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}
