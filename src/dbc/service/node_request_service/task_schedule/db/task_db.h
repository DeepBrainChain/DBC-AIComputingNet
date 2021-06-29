#ifndef DBC_TASK_DB_HS
#define DBC_TASK_DB_HS

#include <string>
#include <unordered_map>
#include <map>
#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include "task_types.h"

namespace dbc
{
	class TaskDB
	{
	public:
		TaskDB() = default;

		virtual ~TaskDB() = default;

		bool init_db(boost::filesystem::path task_db_path, std::string db_name);
		
		void load_tasks(std::map<std::string, std::shared_ptr<TaskInfo>>& tasks);

		bool delete_task(const std::string& task_id);

		std::shared_ptr<TaskInfo> read_task(const std::string& task_id);

		bool write_task(const std::shared_ptr<TaskInfo>& task);
		
	protected: 
		std::unique_ptr<leveldb::DB> m_db;
	};
}

#endif