#ifndef DBCPROJ_TASKRESOURCE_MANAGER_H
#define DBCPROJ_TASKRESOURCE_MANAGER_H

#include "util/utils.h"

class TaskResourceManager {
public:
    TaskResourceManager() = default;

    virtual ~TaskResourceManager() = default;

    void init(const std::vector<std::string>& tasks);

    const DeviceCpu& GetTaskCpu(const std::string& task_id);

    const std::map<std::string, DeviceGpu>& GetTaskGpu(const std::string& task_id);

    const DeviceMem& GetTaskMem(const std::string& task_id);

    const std::map<int32_t, DeviceDisk>& GetTaskDisk(const std::string& task_id);

    void AddTaskCpu(const std::string& task_id, const DeviceCpu& cpu);

    void AddTaskMem(const std::string& task_id, const DeviceMem& mem);

    void AddTaskGpu(const std::string& task_id, const std::map<std::string, DeviceGpu>& gpus);

    void AddTaskDisk(const std::string& task_id, const std::map<int32_t, DeviceDisk>& disks);

    void Clear(const std::string& task_id);

    // test
    void print_cpu(const std::string& task_id);
    void print_gpu(const std::string& task_id);
    void print_mem(const std::string& task_id);
    void print_disk(const std::string& task_id);

private:
    std::string disk_type(const std::string& path = "/data");

private:
    std::map<std::string, DeviceCpu> m_mpTaskCpu;
    std::map<std::string, std::map<std::string, DeviceGpu>> m_mpTaskGpu;
    std::map<std::string, DeviceMem> m_mpTaskMem; // KB
    std::map<std::string, std::map<int32_t, DeviceDisk>> m_mpTaskDisk; // MB (1: 是初始创建的数据盘)
};

#endif //DBCPROJ_TASKRESOURCE_MANAGER_H
