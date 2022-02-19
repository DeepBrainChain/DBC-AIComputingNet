#ifndef DBC_MESSAGE_TO_BYTE_ENCODER_H
#define DBC_MESSAGE_TO_BYTE_ENCODER_H

#include <memory>
#include "encoder.h"
#include "network/protocol/thrift_binary.h"
#include "log/log.h"

class message_to_byte_encoder : public encoder
{
public:
    virtual encode_status encode(dbc::network::channel_handler_context &ctx, dbc::network::message & msg, byte_buf &out)
    {
        std::shared_ptr<dbc::network::binary_protocol> proto = std::make_shared<dbc::network::binary_protocol>(&out);

        try
        {
            msg.write(proto.get());
        }
        catch (std::exception &e)
        {
            LOG_ERROR << "matrix encode message error: " << e.what();
            return ENCODE_ERROR;
        }
        catch (...)
        {
            LOG_ERROR << "matrix encode message error!";
            return ENCODE_ERROR;
        }

        return ENCODERR_SUCCESS;
    }
};

#endif
