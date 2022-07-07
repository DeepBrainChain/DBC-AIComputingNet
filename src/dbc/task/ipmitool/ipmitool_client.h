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

};

#endif // DBC_IPMITOOL_CLIENT_H