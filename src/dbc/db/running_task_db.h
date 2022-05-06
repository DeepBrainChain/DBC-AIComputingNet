#ifndef DBC_RUNNING_TASK_DB_H
#define DBC_RUNNING_TASK_DB_H

#include "util/utils.h"
#include <leveldb/db.h>
#include <boost/filesystem.hpp>

namespace bfs = boost::filesystem;

class RunningTaskDB {
public:
	RunningTaskDB() = default;

	virtual ~RunningTaskDB() = default;

	bool init_db(boost::filesystem::path db_file_path, std::string db_name);

	void load_datas(std::vector<std::string>& datas);

	void load_datas(std::set<std::string>& datas);

	bool is_exist(const std::string& task_id);

	bool write_data(const std::string& task_id);

	bool delete_data(const std::string& task_id);

	void update(std::vector<std::string>& task_ids);

	void update(std::set<std::string>& task_ids);

	void clear();

protected:
	std::unique_ptr<leveldb::DB> m_db;
};

#endif
