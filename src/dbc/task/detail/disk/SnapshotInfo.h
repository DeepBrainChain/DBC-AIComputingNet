#ifndef DBC_SNAPSHOT_INFO_H
#define DBC_SNAPSHOT_INFO_H

#include "util/utils.h"
#include "db/db_types/db_task_snapshot_types.h"
#include "db/task_snapshot_db.h"

enum class SnapshotStatus {
	SS_None,
	SS_Creating, //正在创建
	SS_Uploading //正在上传
};

static std::string snapshot_status_string(SnapshotStatus st) {
	std::string str_status = "none";

	switch (st) {
	case SnapshotStatus::SS_None:
		str_status = "none";
		break;
	case SnapshotStatus::SS_Creating:
		str_status = "creating";
		break;
	case SnapshotStatus::SS_Uploading:
		str_status = "uploading";
		break;
	default:
		break;
	}

	return str_status;
}

class SnapshotInfo {
public:
	friend class TaskDiskManager;

	SnapshotInfo() {
		m_db_info = std::make_shared<dbc::db_snapshot_info>();
	}

	virtual ~SnapshotInfo() = default;
	
	std::string getTaskId() const {
		RwMutex::ReadLock rlcok(m_mtx);
		return m_db_info->task_id;
	}

	void setTaskId(const std::string& task_id) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_task_id(task_id);
	}

	std::vector<dbc::snapshot_info> getSnapshots() {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->snapshots;
	}

	void addSnapshot(const std::string& snapshot_name, const std::string& snapshot_file, 
		int64_t create_time, const std::string& desc) {
		RwMutex::WriteLock wlock(m_mtx);
		dbc::snapshot_info snapshotinfo;
		snapshotinfo.name = snapshot_name;
		snapshotinfo.file = snapshot_file;
		snapshotinfo.create_time = create_time;
		snapshotinfo.desc = desc;
		m_db_info->snapshots.push_back(snapshotinfo);
		m_snapshots_status.insert({ snapshot_name, SnapshotStatus::SS_None });
	}

	void addSnapshot(const dbc::snapshot_info& snapshotinfo) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->snapshots.push_back(snapshotinfo);
		m_snapshots_status.insert({ snapshotinfo.name, SnapshotStatus::SS_None });
	}

	void delSnapshot(const std::string& snapshot_name) {
		RwMutex::WriteLock wlock(m_mtx);
		auto& snapshots = m_db_info->snapshots;
		for (auto iter = snapshots.begin(); iter != snapshots.end(); iter++) {
			if (iter->name == snapshot_name) {
				snapshots.erase(iter);
				break;
			}
		}

		m_snapshots_status.erase(snapshot_name);
	}

	SnapshotStatus getSnapshotStatus(const std::string& snapshot_name) const {
		RwMutex::ReadLock rlock(m_mtx);
		auto iter = m_snapshots_status.find(snapshot_name);
		if (iter != m_snapshots_status.end()) {
			return iter->second;
		}
		else {
			return SnapshotStatus::SS_None;
		}
	}

	void setSnapshotStatus(const std::string& snapshot_name, SnapshotStatus status) {
		RwMutex::WriteLock wlock(m_mtx);
		m_snapshots_status[snapshot_name] = status;
	}

	void deleteFromDB(TaskSnapshotDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.delete_data(m_db_info->task_id);
	}

	void updateToDB(TaskSnapshotDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.write_data(m_db_info);
	}

private:
	mutable RwMutex m_mtx;
	std::map<std::string, SnapshotStatus> m_snapshots_status;
	std::shared_ptr<dbc::db_snapshot_info> m_db_info;
};

#endif