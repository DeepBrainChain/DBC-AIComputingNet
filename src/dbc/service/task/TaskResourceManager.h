#ifndef DBC_TASKRESOURCE_MANAGER_H
#define DBC_TASKRESOURCE_MANAGER_H

#include "util/utils.h"

struct TaskResource {
    // cpu
    int32_t cpu_sockets = 0;
    int32_t cpu_cores = 0;
    int32_t cpu_threads = 0;

    int32_t total_cores() const;

    // gpu
    std::map<std::string, std::list<std::string>> gpus; // <gpu_id, gpu_devices>

    static std::string parse_gpu_device_bus(const std::string& id);
    static std::string parse_gpu_device_slot(const std::string& id);
    static std::string parse_gpu_device_function(const std::string& id);

    // mem (KB)
    uint64_t mem_size = 0;

    //disk (KB)
    uint64_t disk_system_size = 0; //系统盘
    std::map<int32_t, uint64_t> disks; //[1]: 是初始创建的数据盘, [2]、[3]、、依次为挂载盘  (不包括系统盘)
};

class TaskResourceManager : public Singleton<TaskResourceManager> {
public:
    TaskResourceManager() = default;

    virtual ~TaskResourceManager() = default;

    bool init(const std::vector<std::string>& taskids);

    std::shared_ptr<TaskResource> getTaskResource(const std::string& task_id) const;

    void addTaskResource(const std::string& task_id, const std::shared_ptr<TaskResource>& resource);

    void delTaskResource(const std::string& task_id);

private:
    mutable RwMutex m_mtx;
    std::map<std::string, std::shared_ptr<TaskResource> > m_task_resource;
};

typedef TaskResourceManager TaskResourceMgr;

#endif
