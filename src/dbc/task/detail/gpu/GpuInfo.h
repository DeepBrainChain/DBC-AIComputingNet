#ifndef DBC_GPU_INFO_H
#define DBC_GPU_INFO_H

#include "util/utils.h"

class GpuInfo {
public:
	friend class TaskGpuManager;

	static std::string parse_gpu_device_bus(const std::string& id) {
		auto pos = id.find(':');
		if (pos != std::string::npos) {
			return id.substr(0, pos);
		}
		else {
			return "";
		}
	}

	static std::string parse_gpu_device_slot(const std::string& id) {
		auto pos1 = id.find(':');
		auto pos2 = id.find('.');
		if (pos1 != std::string::npos && pos2 != std::string::npos) {
			return id.substr(pos1 + 1, pos2 - pos1 - 1);
		}
		else {
			return "";
		}
	}

	static std::string parse_gpu_device_function(const std::string& id) {
		auto pos = id.find('.');
		if (pos != std::string::npos) {
			return id.substr(pos + 1);
		}
		else {
			return "";
		}
	}

	std::string getId() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_id;
	}

	void setId(const std::string& id) {
		RwMutex::WriteLock wlock(m_mtx);
		m_id = id;
	}

	std::list<std::string> getDeviceIds() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_device_ids;
	}

	void addDeviceId(const std::string& id) {
		RwMutex::WriteLock wlock(m_mtx);
		m_device_ids.push_back(id);
	}

private:
	mutable RwMutex m_mtx;
    std::string m_id;
    std::list<std::string> m_device_ids; // contain gpu_id self
};

#endif
