#include "task_iptable_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

namespace fs = boost::filesystem;

bool TaskIpTableDB::init_db(boost::filesystem::path task_db_path, std::string db_name)
{
    leveldb::DB* db = nullptr;
    leveldb::Options  options;
    options.create_if_missing = true;

    if (!fs::exists(task_db_path))
    {
        LOG_INFO << "db directory path does not exist and create db directory" << task_db_path;
        fs::create_directory(task_db_path);
    }

    if (!fs::is_directory(task_db_path))
    {
        LOG_INFO << "db directory path does not exist and exit" << task_db_path;
        return false;
    }

    task_db_path /= fs::path(db_name);

    leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
    if (!status.ok())
    {
        LOG_ERROR << "init task_iptable db error: " << status.ToString();
        return false;
    }

    m_db.reset(db);

    return true;
}

void TaskIpTableDB::load_iptables(std::map<std::string, std::shared_ptr<dbc::TaskIpTable>>& task_iptables)
{
    std::unique_ptr<leveldb::Iterator> it;
    it.reset(m_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::shared_ptr<dbc::TaskIpTable> task = std::make_shared<dbc::TaskIpTable>();
        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
        dbc::network::binary_protocol proto(task_buf.get());
        task->read(&proto);

        task_iptables.insert({ task->task_id, task });
    }
}

bool TaskIpTableDB::delete_iptable(const std::string& task_id)
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

std::shared_ptr<dbc::TaskIpTable> TaskIpTableDB::read_iptable(const std::string& task_id) {
    if (task_id.empty())
        return nullptr;

    std::string val;
    leveldb::Status status = m_db->Get(leveldb::ReadOptions(), task_id, &val);
    if (status.ok()) {
        std::shared_ptr<dbc::TaskIpTable> task = std::make_shared<dbc::TaskIpTable>();
        std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
        task_buf->write_to_byte_buf(val.c_str(), val.size());
        dbc::network::binary_protocol proto(task_buf.get());
        task->read(&proto);
        return task;
    }
    else {
        return nullptr;
    }
}

bool TaskIpTableDB::write_iptable(const std::shared_ptr<dbc::TaskIpTable>& task_iptable)
{
    std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
    dbc::network::binary_protocol proto(out_buf.get());
    task_iptable->write(&proto);

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
    leveldb::Status status = m_db->Put(write_options, task_iptable->task_id, slice);
    if (status.ok()) {
        return true;
    }
    else {
        LOG_INFO << "task_iptable db put task failed: " << task_iptable->task_id;
        return false;
    }
}
