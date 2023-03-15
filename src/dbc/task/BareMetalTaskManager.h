#ifndef DBC_BARE_METAL_TASK_MANAGER_H
#define DBC_BARE_METAL_TASK_MANAGER_H

#include "bare_metal/bare_metal_client_base.h"

class BareMetalTaskManager : public Singleton<BareMetalTaskManager> {
public:
    BareMetalTaskManager();

    virtual ~BareMetalTaskManager();

    FResult Init();

    void Exit();

    FResult PowerControl(const std::string& node_id,
                         const std::string& command);

    FResult SetBootDeviceOrder(const std::string& node_id,
                               const std::string& device);

    void PruneNode(const std::string& node_id);

    void PruneNodes();

protected:
    bool stopped_;
    std::shared_ptr<BareMetalClientBase> bare_metal_client_;
};

#endif  // DBC_BARE_METAL_TASK_MANAGER_H
