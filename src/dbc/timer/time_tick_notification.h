#ifndef DBC_TIME_TICK_NOTIFICATION_H
#define DBC_TIME_TICK_NOTIFICATION_H

#include "network/protocol/protocol.h"

class time_tick_notification : public network::msg_base
{
public:
    uint64_t time_tick;
};

#endif
