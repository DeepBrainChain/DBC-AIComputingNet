#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include "network/protocol/net_message.h"
#include "network/channel/channel_handler_context.h"

enum decode_status
{
    DECODERR_SUCCESS = 0,
    DECODE_NEED_MORE_DATA,
    DECODE_UNKNOWN_MSG,
    DECODE_ERROR
};

class decoder
{
public:
    virtual decode_status decode(network::channel_handler_context &ctx, 
        std::shared_ptr<network::message> &message) = 0;
};

#endif
