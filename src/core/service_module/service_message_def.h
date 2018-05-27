/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   service_message_def.h
* description    :   service message definition
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include "common.h"
#include "service_message.h"
#include "socket_id.h"
#include <boost/asio.hpp>

using namespace boost::asio::ip;

namespace matrix
{
    namespace core
    {
        //timer tick msg
        class timer_click_msg : public message
        {
        public:
            uint64_t cur_tick;
        };

        class tcp_socket_channel_error_msg : public message
        {
        public:
            tcp::endpoint ep;
        };

        enum CLIENT_CONNECT_STATUS
        {
            CLIENT_CONNECT_SUCCESS      =   0,
            CLIENT_CONNECT_ERROR
        };

        class client_tcp_connect_notification : public message
        {
        public:
            tcp::endpoint ep;
            CLIENT_CONNECT_STATUS status;
        };

    }

}