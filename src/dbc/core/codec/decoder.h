/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        decoder.h
* description    decoder interface class
* date                  :   2018.01.20
* author            Bruce Feng
**********************************************************************************/
#pragma once

#include "service_message.h"
#include "channel_handler_context.h"


namespace matrix
{
    namespace core
    {
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

            //virtual decode_status decode(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &message) = 0;
            virtual decode_status decode(channel_handler_context &ctx, std::shared_ptr<message> &message) = 0;
        };

    }

}
