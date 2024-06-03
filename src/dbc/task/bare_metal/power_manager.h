#ifndef FOOL_POWER_MANAGER_H
#define FOOL_POWER_MANAGER_H

#include "bare_metal_client_base.h"

namespace fool {

class PowerManager : public BareMetalClientBase {
public:
    PowerManager();

    ~PowerManager() override;

    FResult Init() override;

    void Exit() override;

    FResult PowerControl(const std::string& node_id,
                         const std::string& command) override;

    FResult SetBootDeviceOrder(const std::string& node_id,
                               const std::string& device) override;

private:
    std::string power_manager_tool_;
};

}  // namespace fool

#endif  // FOOL_POWER_MANAGER_H