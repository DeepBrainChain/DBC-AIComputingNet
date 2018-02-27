/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºmatrix_server_socket_channel_handler.cpp
* description    £ºdbc socket channel handler for dbc network protocol layer
* date                  : 2018.02.27
* author            £ºBruce Feng
**********************************************************************************/
#include "matrix_client_socket_channel_handler.h"

namespace matrix
{
    namespace service_core
    {

        matrix_client_socket_channel_handler::matrix_client_socket_channel_handler(channel *ch)
            : matrix_socket_channel_handler(ch)
        {
            m_timer_handler =
                [&](const boost::system::error_code & error)
            {
                if (error)
                {
                    LOG_ERROR << "matrix client socket channel handler timer error: " << error;
                    m_channel->on_error();
                    return;
                }

                //login success and no message, send shake hand
                if (true == m_login_success && false == m_has_message)
                {
                    send_shake_hand_req();
                }

                //reset
                reset_has_message();

                //async wait
                m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
                m_shake_hand_timer.async_wait(m_timer_handler);
            };
        }

        int32_t matrix_client_socket_channel_handler::on_after_msg_received(message &msg)
        {
            if (false == m_login_success && VER_RESP != msg.get_name())
            {
                LOG_ERROR << "matrix client socket channel received error message: " << msg.get_name() << " , while not login success" << msg.header.src_sid.to_string();
                m_channel->on_error();
                return E_SUCCESS;
            }

            if (VER_RESP == msg.get_name())
            {
                if (false == m_login_success)
                {
                    m_login_success = true;
                    start_shake_hand_timer();

                    LOG_DEBUG << "matrix client socket channel handler start shake hand timer, socket number: " << m_channel->id().get_id();
                    return E_SUCCESS;
                }
                else
                {
                    LOG_ERROR << "matrix client socket channel handler received duplicated VER_RESP" << msg.header.src_sid.to_string();
                    m_channel->on_error();
                    return E_DEFAULT;
                }
            }

            return E_SUCCESS;
        }

        void matrix_client_socket_channel_handler::send_shake_hand_req()
        {
            //shake_hand_req 
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::shake_hand_req> req_content = std::make_shared<matrix::service_core::shake_hand_req>();

            //header
            req_content->header.length = 0;
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = SHAKE_HAND_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(SHAKE_HAND_REQ);
            m_channel->write(req_msg);
        }

    }

}