#ifndef DBCPROJ_TASKRESOURCE_MANAGER_H
#define DBCPROJ_TASKRESOURCE_MANAGER_H

#include "util/utils.h"

struct TaskResource {
    // cpu
    int32_t physical_cpu = 0;
    int32_t physical_cores_per_cpu = 0;
    int32_t threads_per_cpu = 0;

    int32_t total_cores() const;

    // gpu
    std::map<std::string, std::list<std::string>> gpus; // <gpu_id, gpu_devices>

    static std::string parse_gpu_device_bus(const std::string& id);
    static std::string parse_gpu_device_slot(const std::string& id);
    static std::string parse_gpu_device_function(const std::string& id);

    // mem KB
    uint64_t mem_size = 0;

    //disk MB
    uint64_t disk_system_size = 0; //系统盘
    std::map<int32_t, uint64_t> disks_data; //[1]: 是初始创建的数据盘, [2]、[3]、、依次为挂载盘
};

class TaskResourceManager {
public:
    TaskResourceManager() = default;

    virtual ~TaskResourceManager() = default;

    void init(const std::vector<std::string>& tasks);

    const TaskResource& GetTaskResource(const std::string& task_id) const;

    void AddTaskResource(const std::string& task_id, const TaskResource& resource);

    void Delete(const std::string& task_id);

private:
    std::map<std::string, TaskResource> m_mpTaskResource;
};

#endif //DBCPROJ_TASKRESOURCE_MANAGER_H
