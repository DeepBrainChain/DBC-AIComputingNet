#ifndef DBC_TASK_GPU_MANAGER_h
#define DBC_TASK_GPU_MANAGER_H

#include "../info/TaskInfo.h"
#include "GpuInfo.h"
#include "util/utils.h"

class TaskGpuManager : public Singleton<TaskGpuManager> {
public:
    friend class TaskManager;

    TaskGpuManager() = default;

    virtual ~TaskGpuManager() = default;

    FResult init(const std::vector<std::string>& task_ids);

    std::map<std::string, std::shared_ptr<GpuInfo>> getTaskGpus(
        const std::string& task_id) const {
        RwMutex::ReadLock rlock(m_mtx);
        auto iter = m_task_gpus.find(task_id);
        if (iter != m_task_gpus.end()) {
            return iter->second;
        } else {
            return std::map<std::string, std::shared_ptr<GpuInfo>>();
        }
    }

    void setTaskGpus(
        const std::string& task_id,
        const std::map<std::string, std::shared_ptr<GpuInfo>>& gpus) {
        RwMutex::WriteLock wlock(m_mtx);
        m_task_gpus[task_id] = gpus;
    }

    void add(const std::string& task_id, const std::shared_ptr<GpuInfo>& gpu);

    void del(const std::string& task_id);

    void del(const std::string& task_id, const std::string& gpu_id);

    int32_t getTaskGpusCount(const std::string& task_id);

    void resetGpusByRentOrder(const std::string& task_id,
                              const std::string& rent_order);

    FResult checkXmlGpu(const std::shared_ptr<TaskInfo>& taskinfo,
                        const std::string& rent_order);

private:
    mutable RwMutex m_mtx;
    std::map<std::string, std::map<std::string, std::shared_ptr<GpuInfo>>>
        m_task_gpus;
};

typedef TaskGpuManager TaskGpuMgr;

#endif
