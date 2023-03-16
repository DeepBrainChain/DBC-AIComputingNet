#ifndef DBC_IPMITOOL_CLIENT_H
#define DBC_IPMITOOL_CLIENT_H

#include "bare_metal_client_base.h"

class IpmiToolClient : public BareMetalClientBase {
public:
    IpmiToolClient();

    ~IpmiToolClient() override;

    FResult Init() override;

    void Exit() override;

    FResult PowerControl(const std::string& node_id,
                         const std::string& command) override;

    FResult SetBootDeviceOrder(const std::string& node_id,
                               const std::string& device) override;
};

#endif  // DBC_IPMITOOL_CLIENT_H