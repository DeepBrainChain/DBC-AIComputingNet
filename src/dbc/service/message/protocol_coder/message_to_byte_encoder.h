/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        message_to_byte_encoder.h
* description    message encode to bytes
* date                  :   2018.01.20
* author            Bruce Feng
**********************************************************************************/
#pragma once

#include <memory>
#include "encoder.h"
#include "thrift_binary.h"

namespace matrix
{
    namespace core
    {
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

                return ENCODE_SUCCESS;
            }
        };
    }
}
