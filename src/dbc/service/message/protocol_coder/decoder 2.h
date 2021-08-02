#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include "network/protocol/service_message.h"
#include "network/channel_handler_context.h"

enum decode_status
{
    DECODE_SUCCESS = 0,
    DECODE_NEED_MORE_DATA,
    DECODE_UNKNOWN_MSG,
    DECODE_ERROR
};

class decoder
{
public:
    virtual decode_status decode(dbc::network::channel_handler_context &ctx, std::shared_ptr<dbc::network::message> &message) = 0;
};

#endif
