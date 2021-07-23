#ifndef DBCPROJ_TASKRESOURCE_MANAGER_H
#define DBCPROJ_TASKRESOURCE_MANAGER_H

#include "util/utils.h"

class TaskResourceManager : public Singleton<TaskResourceManager> {
public:
    TaskResourceManager();

    virtual ~TaskResourceManager();

    void init();

private:
    std::map<std::string, DeviceCpu> m_mpTaskCpu;
    std::map<std::string, std::list<DeviceGpu>> m_mpTaskGpu;
    std::map<std::string, DeviceMem> m_mpTaskMem;
    std::map<std::string, DeviceDisk> m_mpTaskDisk;
};

typedef TaskResourceManager TaskResourceMgr;

#endif //DBCPROJ_TASKRESOURCE_MANAGER_H
