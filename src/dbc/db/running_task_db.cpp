#include "running_task_db.h"
#include "log/log.h"
#include "util/memory/byte_buf.h"
#include "network/protocol/thrift_binary.h"

bool RunningTaskDB::init_db(boost::filesystem::path db_file_path, std::string db_name)
{
	leveldb::DB* db = nullptr;
	leveldb::Options  options;
	options.create_if_missing = true;

	if (!bfs::exists(db_file_path))
	{
		LOG_INFO << "db directory path does not exist and create db directory" << db_file_path;
		bfs::create_directory(db_file_path);
	}

	if (!bfs::is_directory(db_file_path))
	{
		LOG_INFO << "db directory path does not exist and exit" << db_file_path;
		return false;
	}

	bfs::path db_file_full_path = db_file_path / bfs::path(db_name);

	leveldb::Status status = leveldb::DB::Open(options, db_file_full_path.generic_string(), &db);
	if (!status.ok())
	{
		LOG_ERROR << "open db error: db=" << db_file_full_path.generic_string() << ", status=" << status.ToString();
		return false;
	}

	m_db.reset(db);

	return true;
}

void RunningTaskDB::load_datas(std::vector<std::string>& datas)
{
	std::unique_ptr<leveldb::Iterator> it;
	it.reset(m_db->NewIterator(leveldb::ReadOptions()));
	for (it->SeekToFirst(); it->Valid(); it->Next())
	{
		std::string id(it->key().data(), it->key().size());
		datas.push_back(id);
	}
}

void RunningTaskDB::load_datas(std::set<std::string>& datas) {
	std::unique_ptr<leveldb::Iterator> it;
	it.reset(m_db->NewIterator(leveldb::ReadOptions()));
	for (it->SeekToFirst(); it->Valid(); it->Next())
	{
		std::string id(it->key().data(), it->key().size());
		datas.insert(id);
	}
}

bool RunningTaskDB::is_exist(const std::string& task_id) {
	if (task_id.empty()) return false;

	std::string val;
	leveldb::Status status = m_db->Get(leveldb::ReadOptions(), task_id, &val);
	if (status.ok()) {
		return true;
	}
	else {
		return false;
	}
}

bool RunningTaskDB::write_data(const std::string& task_id)
{
	leveldb::WriteOptions write_options;
	write_options.sync = true;
	leveldb::Slice slice(task_id.c_str(), task_id.size());
	leveldb::Status status = m_db->Put(write_options, task_id, slice);
	if (status.ok()) {
		return true;
	}
	else {
		LOG_INFO << "put data failed: " << task_id;
		return false;
	}
}

bool RunningTaskDB::delete_data(const std::string& task_id)
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

void RunningTaskDB::update(std::vector<std::string>& task_ids) {
	clear();
	for (auto& id : task_ids) {
		write_data(id);
	}
}

void RunningTaskDB::update(std::set<std::string>& task_ids) {
	clear();
	for (auto& id : task_ids) {
		write_data(id);
	}
}

void RunningTaskDB::clear() {
	std::unique_ptr<leveldb::Iterator> it;
	it.reset(m_db->NewIterator(leveldb::ReadOptions()));

	leveldb::WriteOptions write_options;
	write_options.sync = true;
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		m_db->Delete(write_options, it->key());
	}
}
