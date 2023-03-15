#ifndef DBC_BARE_METAL_CLIENT_BASE_H
#define DBC_BARE_METAL_CLIENT_BASE_H

#include "util/utils.h"

class BareMetalClientBase {
public:
    BareMetalClientBase(){};
    virtual ~BareMetalClientBase(){};

    virtual FResult Init(){};
    virtual void Exit(){};

    virtual FResult PowerControl(const std::string& node_id,
                                 const std::string& command){};

    virtual FResult SetBootDeviceOrder(const std::string& node_id,
                                       const std::string& device){};
};

#endif  // DBC_BARE_METAL_CLIENT_BASE_H
