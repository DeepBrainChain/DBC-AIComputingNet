#ifndef DBC_IPTABLE_INFO_H
#define DBC_IPTABLE_INFO_H

#include "util/utils.h"
#include "db/db_types/db_task_iptable_types.h"
#include "db/task_iptable_db.h"

class IptableInfo {
public:
    friend class TaskIptableManager;

	IptableInfo() {
		m_db_info = std::make_shared<dbc::db_task_iptable>();
	}

    virtual ~IptableInfo() = default;

	std::string getTaskId() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->task_id;
	}

	void setTaskId(const std::string& id) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_task_id(id);
	}

	std::string getHostIP() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->host_ip;
	}

	void setHostIP(const std::string& ip) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_host_ip(ip);
	}

	std::string getTaskLocalIP() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->task_local_ip;
	}

	void setTaskLocalIP(const std::string& ip) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_task_local_ip(ip);
	}

	uint16_t getSSHPort() const {
		RwMutex::ReadLock rlock(m_mtx);
		return (uint16_t) atoi(m_db_info->ssh_port.c_str());
	}

	void setSSHPort(uint16_t port) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_ssh_port(std::to_string(port));
	}

	uint16_t getRDPPort() const {
		RwMutex::ReadLock rlock(m_mtx);
		return (uint16_t) atoi(m_db_info->rdp_port.c_str());
	}

	void setRDPPort(uint16_t port) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_rdp_port(std::to_string(port));
	}

	std::vector<std::string> getCustomPort() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->custom_port;
	}

	void setCustomPort(const std::vector<std::string>& ports) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_custom_port(ports);
	}

	std::string getPublicIP() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->public_ip;
	}

	void setPublicIP(const std::string& ip) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_public_ip(ip);
	}

	void deleteFromDB(TaskIptableDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.delete_data(m_db_info->task_id);
	}

	void updateToDB(TaskIptableDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.write_data(m_db_info);
	}

private:
	mutable RwMutex m_mtx;
    std::shared_ptr<dbc::db_task_iptable> m_db_info;
};

#endif