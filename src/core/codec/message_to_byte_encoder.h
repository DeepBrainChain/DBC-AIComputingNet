/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºmessage_to_byte_encoder.h
* description    £ºmessage encode to bytes
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include <memory>
#include "encoder.h"


namespace matrix
{
    namespace core
    {

        class message_to_byte_encoder : public encoder
        {
        public:

            virtual encode_status encode(channel_handler_context &ctx, message & msg, byte_buf &out)
            {
                std::shared_ptr<binary_protocol> proto(new binary_protocol(&out));

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


