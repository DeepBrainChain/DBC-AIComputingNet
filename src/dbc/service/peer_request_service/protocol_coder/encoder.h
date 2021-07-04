/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        encoder.h
* description    dbc encoder interface class
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

    }

}