#include "task_snapshot_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

bool TaskSnapshotDB::init_db(boost::filesystem::path task_db_path, std::string db_name)
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

void TaskSnapshotDB::load_datas(std::map<std::string, std::shared_ptr<dbc::db_snapshot_info>>& datas)
{
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::db_snapshot_info> ptr = std::make_shared<dbc::db_snapshot_info>();
        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        network::binary_protocol proto(task_buf.get());
        ptr->read(&proto);

        datas.insert({ ptr->task_id, ptr });
    }
}

std::shared_ptr<dbc::db_snapshot_info> TaskSnapshotDB::read_data(const std::string& task_id) {
    if (task_id.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), task_id, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::db_snapshot_info> ptr = std::make_shared<dbc::db_snapshot_info>();
        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(val.c_str(), val.size());
        network::binary_protocol proto(task_buf.get());
        ptr->read(&proto);
        return ptr;
    }
    else {
        return nullptr;
    }
}

bool TaskSnapshotDB::write_data(const std::shared_ptr<dbc::db_snapshot_info>& data)
{
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    network::binary_protocol proto(out_buf.get());
    data->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, data->task_id, slice);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_INFO << "db put data failed: " << data->task_id;
        return false;
    }
}

bool TaskSnapshotDB::delete_data(const std::string& task_id)
{
	if (task_id.empty())
		return true;

	leveldb::WriteOptions write_options;
	write_options.sync = true;
	leveldb::Status status = m_db->Delete(write_options, task_id);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}
