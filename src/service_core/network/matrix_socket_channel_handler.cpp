/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºmatrix_socket_channel_handler.cpp
* description    £ºmatrix socket channel handler
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/
#include "matrix_socket_channel_handler.h"
#include "p2p_net_service.h"
#include "server.h"
#include "tcp_socket_channel.h"


namespace matrix
{
    namespace service_core
    {

        matrix_socket_channel_handler::matrix_socket_channel_handler(std::shared_ptr<channel> ch)
            : m_stopped(false)
            , m_channel(ch)
            , m_coder(new matrix_coder())
            , m_has_message(false)
            , m_login_success(false)
            , m_shake_hand_timer(*(ch->get_io_service()))
            , m_sid(ch->id())
        {}

        matrix_socket_channel_handler::~matrix_socket_channel_handler()
        {
            LOG_DEBUG << "socket channel handler destroyed, " << m_sid.to_string();
        }

        int32_t matrix_socket_channel_handler::stop()
        {
            m_stopped = true;
            stop_shake_hand_timer();

            return E_SUCCESS;
        }

        int32_t matrix_socket_channel_handler::on_read(channel_handler_context &ctx, byte_buf &in)
        {
            //decode until buffer is not enough to do
            while (true)
            {
                shared_ptr<message> msg = std::make_shared<message>();
                decode_status status = m_coder->decode(ctx, in, msg);

                //decode success
                if (DECODE_SUCCESS == status)
                {

                    LOG_DEBUG << "socket channel handler recv msg: " << msg->get_name() << m_sid.to_string();

                    //send to bus
                    if (msg->get_name() != SHAKE_HAND_REQ 
                        && msg->get_name() != SHAKE_HAND_RESP)
                    {
                        msg->header.src_sid = m_sid;
                        TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);
                    }

                    //callback
                    on_after_msg_received(*msg);

                    //has message
                    set_has_message(*msg);

                    continue;
                }
                //not enough, return and next time
                else if (DECODE_LENGTH_IS_NOT_ENOUGH == status)
                {
                    return E_SUCCESS;
                }
                //decode error
                else
                {
                    LOG_ERROR << "matrix socket channel handler on read error and call socket channel on_error, " << m_sid.to_string();
                    //on_error();
                    return E_DEFAULT;
                }
            }
        }

        int32_t matrix_socket_channel_handler::on_write(channel_handler_context &ctx, message &msg, byte_buf &buf)
        {

            LOG_DEBUG << "socket channel handler send msg: " << msg.get_name() << m_sid.to_string();

            encode_status status = m_coder->encode(ctx, msg, buf);
            if (ENCODE_SUCCESS == status)
            {
                //get msg length and net endiuan and fill in
                uint32_t msg_len = buf.get_valid_read_len();
                msg_len = byte_order::hton32(msg_len);
                memcpy(buf.get_read_ptr() + 6, &msg_len, sizeof(msg_len));

                //has message
                set_has_message(msg);

                return E_SUCCESS;
            }
            else if (BUFFER_IS_NOT_ENOUGH_TO_ENCODE == status)
            {
                //this should be found in coding phase
                LOG_ERROR << "matrix_socket_channel_handler encode error, buffer is not enough to encode: " << msg.get_name();
                return E_DEFAULT;
            }
            else
            {
                LOG_ERROR << "matrix_socket_channel_handler encode error: " << msg.get_name();
                return E_DEFAULT;
            }
        }

        int32_t matrix_socket_channel_handler::on_error()
        {
            if (auto ch = m_channel.lock())
            {
                ch->on_error();
            }
            return E_SUCCESS;
        }

        void matrix_socket_channel_handler::start_shake_hand_timer()
        {
            m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
            m_shake_hand_timer.async_wait(m_shake_hand_timer_handler);
            reset_has_message();
        }

        void matrix_socket_channel_handler::stop_shake_hand_timer()
        {
            boost::system::error_code error;

            m_shake_hand_timer.cancel(error);
            if (error)
            {
                LOG_ERROR << "matrix socket channel handler cancel timer error: " << error;
            }
            m_shake_hand_timer_handler = nullptr;

        }

        void matrix_socket_channel_handler::set_has_message(message &msg)
        {
            if (SHAKE_HAND_REQ == msg.get_name()
                || SHAKE_HAND_RESP == msg.get_name()
                || VER_REQ == msg.get_name()
                || VER_RESP == msg.get_name())
            {
                return;
            }

            m_has_message = true;
        }

    }

}