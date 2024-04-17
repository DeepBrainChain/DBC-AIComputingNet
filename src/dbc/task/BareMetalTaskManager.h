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

    void PowerOffOnce(const std::string& node_id);

    void PruneNode(const std::string& node_id);

    void PruneNodes();

protected:
    bool stopped_;
    std::shared_ptr<BareMetalClientBase> bare_metal_client_;
    std::map<std::string, bool> poweroff_once_;
};

#endif  // DBC_BARE_METAL_TASK_MANAGER_H
