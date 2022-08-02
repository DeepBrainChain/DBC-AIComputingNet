#ifndef DBC_IPMITOOL_CLIENT_H
#define DBC_IPMITOOL_CLIENT_H

#include "util/utils.h"

class IpmiToolClient : public Singleton<IpmiToolClient> {
public:
    IpmiToolClient();

    virtual ~IpmiToolClient();

    FResult Init();

    void Exit();

    FResult PowerControl(const std::string& node_id, const std::string& command);

    FResult SetBootDeviceOrder(const std::string& node_id, const std::string& device);

    void PruneNode(const std::string& node_id);

    void PruneNodes();

protected:
    bool stopped = false;

};

#endif // DBC_IPMITOOL_CLIENT_H