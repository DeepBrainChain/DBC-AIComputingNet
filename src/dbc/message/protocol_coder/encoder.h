#ifndef DBC_ENCODER_H
#define DBC_ENCODER_H

#include "network/protocol/net_message.h"
#include "network/channel/channel_handler_context.h"

enum encode_status
{
    ENCODERR_SUCCESS = 0,
    BUFFER_IS_NOT_ENOUGH_TO_ENCODE,
    ENCODE_ERROR
};

class encoder
{
public:
    virtual encode_status encode(network::channel_handler_context &ctx, 
        network::message &msg, byte_buf &out) = 0;
};

#endif
