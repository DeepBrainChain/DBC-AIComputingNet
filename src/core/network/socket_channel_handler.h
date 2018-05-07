/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºsocket_channel_handler.h
* description    £ºsocket channel handler for protocol layer
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include <boost/enable_shared_from_this.hpp>
#include "byte_buf.h"
#include "service_message.h"
#include "channel_handler_context.h"



namespace matrix
{
    namespace core
    {

        class socket_channel_handler : public std::enable_shared_from_this<socket_channel_handler>, public boost::noncopyable
        {
        public:

            virtual ~socket_channel_handler();

            virtual int32_t start() = 0;

            virtual int32_t stop() = 0;

        public:

            virtual int32_t on_read(channel_handler_context &ctx, byte_buf &in) = 0;

            virtual int32_t on_write(channel_handler_context &ctx, message &msg, byte_buf &buf) = 0;

            virtual int32_t on_error() = 0;

        public:

            virtual int32_t on_before_msg_send(message &msg) = 0;

            virtual int32_t on_after_msg_sent(message &msg) = 0;

            virtual int32_t on_before_msg_receive() = 0;

            virtual int32_t on_after_msg_received(message &msg) = 0;

        };

    }

}

