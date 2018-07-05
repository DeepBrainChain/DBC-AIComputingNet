/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   matrix_client_socket_channel_handler.h
* description    :   dbc client socket channel handler for dbc network protocol layer
* date                  :   2018.02.27
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include <boost/asio/steady_timer.hpp>
#include "matrix_socket_channel_handler.h"


using namespace boost::asio;

namespace matrix
{
    namespace service_core
    {
        class matrix_client_socket_channel_handler : public matrix_socket_channel_handler
        {

            typedef void (timer_handler_type)(const boost::system::error_code &);

        public:

            matrix_client_socket_channel_handler(std::shared_ptr<channel> ch);

            virtual ~matrix_client_socket_channel_handler() = default;

            virtual int32_t start();

            static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<channel> ch);

            virtual int32_t on_after_msg_received(message &msg);

        protected:
            virtual void on_shake_hand_timer_expired(const boost::system::error_code& error);
            virtual void start_shake_hand_timer_ext();

        protected:

            void send_shake_hand_req();

        };

    }

}
