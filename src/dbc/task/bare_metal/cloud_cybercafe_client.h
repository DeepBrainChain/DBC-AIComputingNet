#ifndef DBC_CLOUD_CYBERCAFE_CLIENT_H
#define DBC_CLOUD_CYBERCAFE_CLIENT_H

#include "bare_metal_client_base.h"
#include "network/utils/io_service_pool.h"

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
    std::string m_host;
    uint32_t m_port;
    std::shared_ptr<network::io_service_pool> m_io_service_pool = nullptr;
};

#endif  // DBC_CLOUD_CYBERCAFE_CLIENT_H
