#ifndef DBC_CLOUD_CYBERCAFE_CLIENT_H
#define DBC_CLOUD_CYBERCAFE_CLIENT_H

#include "bare_metal_client_base.h"

class CloudCybercafeClient : public BareMetalClientBase {
public:
    CloudCybercafeClient();
    ~CloudCybercafeClient() override;

    FResult Init() override;

    void Exit() override;

    FResult PowerControl(const std::string& node_id,
                         const std::string& command) override;

    FResult SetBootDeviceOrder(const std::string& node_id,
                               const std::string& device) override;

private:
    std::string host;
    std::string port;
};

#endif  // DBC_CLOUD_CYBERCAFE_CLIENT_H
