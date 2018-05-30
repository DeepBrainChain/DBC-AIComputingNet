/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   matrix_server_socket_channel_handler.cpp
* description    :   dbc socket channel handler for dbc network protocol layer
* date                  :   2018.02.27
* author            :   Bruce Feng
**********************************************************************************/
#include "matrix_client_socket_channel_handler.h"
#include "topic_manager.h"

namespace matrix
{
    namespace service_core
    {

        matrix_client_socket_channel_handler::matrix_client_socket_channel_handler(std::shared_ptr<channel> ch)
            : matrix_socket_channel_handler(ch)
        {
 
        }

        int32_t matrix_client_socket_channel_handler::start()
        {
            auto self(shared_from_this());

            m_shake_hand_timer_handler =
                [this, self](const boost::system::error_code & error)
            {
                if (true == m_stopped)
                {
                    LOG_DEBUG << "matrix_client_socket_channel_handler has been stopped and shake_hand_timer_handler exit directly" << m_sid.to_string();
                    return;
                }

                if (error)
                {
                    //aborted, maybe cancel triggered
                    if (boost::asio::error::operation_aborted == error.value())
                    {
                        LOG_DEBUG << "matrix client socket channel handler timer aborted.";
                        return;
                    }

                    LOG_ERROR << "matrix client socket channel handler timer error: " << error.value() << " " << error.message() << m_sid.to_string();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return;
                }

                //login success and no message, time out and send shake hand
                if (true == m_login_success && false == m_has_message)
                {
                    send_shake_hand_req();
                }

                //reset
                reset_has_message();

                //next time async wait
                m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
                m_shake_hand_timer.async_wait(m_shake_hand_timer_handler);
            };

            return E_SUCCESS;
        }

        int32_t matrix_client_socket_channel_handler::on_after_msg_received(message &msg)
        {
            if (false == m_login_success && VER_RESP != msg.get_name())
            {
                LOG_ERROR << "matrix client socket channel received error message: " << msg.get_name() << " , while not login success" << msg.header.src_sid.to_string();
                /*if (auto ch = m_channel.lock())
                {
                    ch->on_error();
                }
                return E_SUCCESS;*/
                return E_DEFAULT;
            }

            if (VER_RESP == msg.get_name())
            {
                if (false == m_login_success)
                {
                    m_login_success = true;
                    start_shake_hand_timer();

                    LOG_DEBUG << "matrix client socket channel handler start shake hand timer, " << m_sid.to_string();
                    return E_SUCCESS;
                }
                else
                {
                    LOG_ERROR << "matrix client socket channel handler received duplicated VER_RESP" << m_sid.to_string();
                    /*if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }*/
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        std::shared_ptr<socket_channel_handler> matrix_client_socket_channel_handler::create(std::shared_ptr<channel> ch)
        {
            shared_ptr<socket_channel_handler> handler(new matrix_client_socket_channel_handler(ch));
            return handler->shared_from_this();
        }

        void matrix_client_socket_channel_handler::send_shake_hand_req()
        {
            //shake_hand_req 
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::shake_hand_req> req_content = std::make_shared<matrix::service_core::shake_hand_req>();

            //header
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(SHAKE_HAND_REQ);

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(SHAKE_HAND_REQ);

            if (auto ch = m_channel.lock())
            {
                ch->write(req_msg);
            }
        }

    }

}