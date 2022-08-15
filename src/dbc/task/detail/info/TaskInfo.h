#ifndef DBC_TASK_INFO_H
#define DBC_TASK_INFO_H

#include "util/utils.h"
#include "db/db_types/db_task_info_types.h"
#include "db/task_info_db.h"

enum class TaskStatus {
	TS_None,

	//虚拟机状态
	TS_Task_Shutoff,            //已关闭
	TS_Task_Running,            //正在运行
	TS_Task_PMSuspend,          //睡眠状态

	//执行过程状态
	TS_Task_Creating,           //正在创建
	TS_Task_Starting,           //正在启动
	TS_Task_BeingShutdown,      //正在关机
	TS_Task_BeingPoweroff,      //正在断电
	TS_Task_Restarting,         //正在重启
	TS_Task_Resetting,          //正在reset
	TS_Task_Deleting,           //正在删除

	TS_Disk_Resizing,           //正在扩容
	TS_Disk_Adding,             //正在添加磁盘
	TS_Disk_Deleting,           //正在删除磁盘

	TS_Snapshot_Creating,       //正在创建快照

	//执行结果错误
	TS_Error,
	TS_CreateTaskError,         //创建task错误
	TS_StartTaskError,          //启动task错误
	TS_ShutdownTaskError,       //关机错误
    TS_PoweroffTaskError,       //强制断电错误
	TS_RestartTaskError,        //重启task错误
	TS_ResetTaskError,          //重置task错误
	TS_DeleteTaskError,         //删除task错误

    TS_ResizeDiskError,         //磁盘扩容错误
    TS_AddDiskError,            //添加新数据盘错误
    TS_DeleteDiskError,         //删除数据盘错误

	TS_CreateSnapshotError      //创建并上传快照错误
};

static std::string task_status_string(TaskStatus status) {
	std::string str_status = "none";

	switch (status) {
    case TaskStatus::TS_None:
        str_status = "none";
        break;
    case TaskStatus::TS_Task_Shutoff:
        str_status = "closed";
        break;
    case TaskStatus::TS_Task_Running:
        str_status = "running";
        break;
    case TaskStatus::TS_Task_PMSuspend:
        str_status = "pmsuspend";
        break;
    case TaskStatus::TS_Task_Creating:
        str_status = "creating";
        break;
    case TaskStatus::TS_Task_Starting:
        str_status = "starting";
        break;
    case TaskStatus::TS_Task_BeingShutdown:
        str_status = "being shutdown";
        break;
    case TaskStatus::TS_Task_BeingPoweroff:
        str_status = "being poweroff";
        break;
    case TaskStatus::TS_Task_Restarting:
        str_status = "restarting";
        break;
    case TaskStatus::TS_Task_Resetting:
        str_status = "resetting";
        break;
    case TaskStatus::TS_Task_Deleting:
        str_status = "deleting";
        break;
    case TaskStatus::TS_Disk_Resizing:
        str_status = "disk resizing";
        break;
    case TaskStatus::TS_Disk_Adding:
        str_status = "disk adding";
        break;
    case TaskStatus::TS_Disk_Deleting:
        str_status = "disk deleting";
        break;
    case TaskStatus::TS_Snapshot_Creating:
        str_status = "snapshot creating";
        break;
    case TaskStatus::TS_Error:
        str_status = "error";
        break;
    case TaskStatus::TS_CreateTaskError:
        str_status = "create error";
        break;
    case TaskStatus::TS_StartTaskError:
        str_status = "start error";
        break;
    case TaskStatus::TS_ShutdownTaskError:
        str_status = "shutdown error";
        break;
    case TaskStatus::TS_PoweroffTaskError:
        str_status = "poweroff error";
        break;
    case TaskStatus::TS_RestartTaskError:
        str_status = "restart error";
        break;
    case TaskStatus::TS_ResetTaskError:
        str_status = "reset error";
        break;
    case TaskStatus::TS_DeleteTaskError:
        str_status = "delete error";
        break;
    case TaskStatus::TS_ResizeDiskError:
        str_status = "resize disk error";
        break;
    case TaskStatus::TS_AddDiskError:
        str_status = "add disk error";
        break;
    case TaskStatus::TS_DeleteDiskError:
        str_status = "delete disk error";
        break;
    case TaskStatus::TS_CreateSnapshotError:
        str_status = "create snapshot error";
        break;
	default:
		break;
	}

	return str_status;
}


class TaskInfo {
public:
    friend class TaskInfoManager;

    TaskInfo() {
        m_db_info = std::make_shared<dbc::db_task_info>();
    }
    
    virtual ~TaskInfo() = default;

    std::string getTaskId() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->task_id;
    }

    void setTaskId(const std::string& id) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_task_id(id);
    }

    std::string getImageName() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->image_name;
    }

    void setImageName(const std::string& name) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_image_name(name);
    }

    std::string getLoginUsername() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->login_username;
    }

    void setLoginUsername(const std::string& username) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_login_username(username);
    }

    std::string getLoginPassword() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->login_password;
    }

    void setLoginPassword(const std::string& password) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_login_password(password);
    }

    uint16_t getSSHPort() const {
        RwMutex::ReadLock rlock(m_mtx);
        return (uint16_t) atoi(m_db_info->ssh_port.c_str());
    }

    void setSSHPort(uint16_t port) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_ssh_port(std::to_string(port));
    }

    int64_t getCreateTime() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->create_time;
    }

    void setCreateTime(int64_t datetime) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_create_time(datetime);
    }

    std::string getOperationSystem() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->operation_system;
    }

    void setOperationSystem(const std::string& os) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_operation_system(os);
    }

    std::string getBiosMode() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->bios_mode;
    }

    void setBiosMode(const std::string& biosmode) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_bios_mode(biosmode);
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

    std::vector<std::string> getMulticast() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->multicast;
    }

    void setMulticast(const std::vector<std::string>& muls) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_multicast(muls);
    }

	std::string getDesc() const {
		RwMutex::ReadLock rlock(m_mtx);
		return m_db_info->desc;
	}

	void setDesc(const std::string& desc) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_desc(desc);
	}

    std::string getVdaRootBackfile() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->vda_rootbackfile;
    }

    void setVdaRootBackfile(const std::string& file) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_vda_rootbackfile(file);
    }

    std::string getNetworkName() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->network_name;
    }

    void setNetworkName(const std::string& name) {
		RwMutex::WriteLock wlock(m_mtx);
		m_db_info->__set_network_name(name);
    }

    std::string getPublicIP() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->public_ip;
    }

    void setPublicIP(const std::string& ip) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_public_ip(ip);
    }

    std::vector<std::string> getNwfilter() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->nwfilter;
    }

    void setNwfilter(const std::vector<std::string>& nwfilter) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_nwfilter(nwfilter);
    }

    int32_t getCpuSockets() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_cpu_sockets;
    }

    void setCpuSockets(int32_t n) {
        RwMutex::WriteLock wlock(m_mtx);
        m_cpu_sockets = n;
    }

    int32_t getCpuCoresPerSocket() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_cpu_cores_per_socket;
    }

    void setCpuCoresPerSocket(int32_t n) {
        RwMutex::WriteLock wlock(m_mtx);
        m_cpu_cores_per_socket = n;
    }

    int32_t getCpuThreadsPerCore() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_cpu_threads_per_core;
    }

    void setCpuThreadsPerCore(int32_t n) {
        RwMutex::WriteLock wlock(m_mtx);
        m_cpu_threads_per_core = n;
    }

    int32_t getTotalCores() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_cpu_sockets * m_cpu_cores_per_socket * m_cpu_threads_per_core;
    }

    int64_t getMemSize() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_mem_size;
    }

    void setMemSize(int64_t n) {
        RwMutex::WriteLock wlock(m_mtx);
        m_mem_size = n;
    }

    uint16_t getVncPort() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_vnc_port;
    }

    void setVncPort(uint16_t port) {
        RwMutex::WriteLock wlock(m_mtx);
        m_vnc_port = port;
    }

    std::string getVncPassword() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_vnc_password;
    }

    void setVncPassword(const std::string& password) {
        RwMutex::WriteLock wlock(m_mtx);
        m_vnc_password = password;
    }

    TaskStatus getTaskStatus() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_status;
    }

    void setTaskStatus(const TaskStatus& status) {
        RwMutex::WriteLock wlock(m_mtx);
        m_status = status;
    }

    std::string getInterfaceModelType() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_db_info->interface_model_type;
    }

    void setInterfaceModelType(const std::string& type) {
        RwMutex::WriteLock wlock(m_mtx);
        m_db_info->__set_interface_model_type(type);
    }

    void deleteFromDB(TaskInfoDB& db) {
        RwMutex::WriteLock wlock(m_mtx);
        db.delete_data(m_db_info->task_id);
    }

	void updateToDB(TaskInfoDB& db) {
		RwMutex::WriteLock wlock(m_mtx);
		db.write_data(m_db_info);
	}

private:
    mutable RwMutex m_mtx;
    std::shared_ptr<dbc::db_task_info> m_db_info; 
    // cpu
    int32_t m_cpu_sockets = 0;
    int32_t m_cpu_cores_per_socket = 0;
    int32_t m_cpu_threads_per_core = 0;
    // mem (KB)
    int64_t m_mem_size = 0;
    // vnc
    uint16_t m_vnc_port;
    std::string m_vnc_password;
    // status
    TaskStatus m_status = TaskStatus::TS_None;
};

#endif