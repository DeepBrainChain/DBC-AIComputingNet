/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   matrix_server_socket_channel_handler.h
* description    :   dbc socket channel handler for dbc network protocol layer
* date                  :   2018.02.27
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include "matrix_socket_channel_handler.h"


using namespace boost::asio;

namespace matrix
{
    namespace service_core
    {

        class matrix_server_socket_channel_handler : public matrix_socket_channel_handler
        {
        public:

            matrix_server_socket_channel_handler(std::shared_ptr<channel> ch);

            virtual ~matrix_server_socket_channel_handler() = default;

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t on_after_msg_received(message &msg);

            virtual int32_t on_after_msg_sent(message &msg);

            virtual int32_t on_before_msg_receive();

            static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<channel> ch);

        protected:

            //I think don't worry about multi thread visit
            void reset_lost_shake_hand_count() { m_lost_shake_hand_count = 0; }

            void send_shake_hand_resp();

            virtual void start_wait_ver_req_timer();

            virtual void stop_wait_ver_req_timer();

            void on_shake_hand_timer_expired(const boost::system::error_code& error);
            virtual void start_shake_hand_timer_ext();

            void on_ver_req_timer_expired(const boost::system::error_code& error);

        protected:

            uint16_t m_lost_shake_hand_count;

            uint16_t m_lost_shake_hand_count_max;

            steady_timer m_wait_ver_req_timer;

        private:
            bool m_recv_ver_req;

        };

    }

}


