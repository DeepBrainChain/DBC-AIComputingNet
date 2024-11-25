#ifndef DBC_DEEPLINK_PARTICIPANT_H
#define DBC_DEEPLINK_PARTICIPANT_H

#include "deeplink_lan_message.hpp"

class deeplink_participant {
public:
    virtual ~deeplink_participant() {}
    virtual void close() = 0;
    virtual void deliver(const deeplink_lan_message& msg) = 0;
    virtual void get_device_info(std::string& device_id,
                                 std::string& password) = 0;
    virtual void set_device_info(const std::string& device_id,
                                 const std::string& password) = 0;
};

#endif  // DBC_DEEPLINK_PARTICIPANT_H
