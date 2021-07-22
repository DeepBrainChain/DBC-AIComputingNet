#ifndef DBC_ENCODER_H
#define DBC_ENCODER_H

#include "network/protocol/service_message.h"
#include "network/channel_handler_context.h"

enum encode_status
{
    ENCODE_SUCCESS = 0,
    BUFFER_IS_NOT_ENOUGH_TO_ENCODE,
    ENCODE_ERROR
};

class encoder
{
public:
    virtual encode_status encode(dbc::network::channel_handler_context &ctx, dbc::network::message &msg, byte_buf &out) = 0;
};

#endif
