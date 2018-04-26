/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºmatrix_client_socket_channel_handler.cpp
* description    £ºdbc socket channel handler for dbc network protocol layer
* date                  : 2018.02.27
* author            £ºBruce Feng
**********************************************************************************/
#include "matrix_server_socket_channel_handler.h"

namespace matrix
{
    namespace service_core
    {

        matrix_server_socket_channel_handler::matrix_server_socket_channel_handler(std::shared_ptr<channel> ch)
            : matrix_socket_channel_handler(ch)
            , m_lost_shake_hand_count(0)
            , m_lost_shake_hand_count_max(LOST_SHAKE_HAND_COUNT_MAX)
            , m_wait_ver_req_timer(*(ch->get_io_service()))
        {

        }

        int32_t matrix_server_socket_channel_handler::start()
        {
            auto self(shared_from_this());

            //shake hand timer
            m_shake_hand_timer_handler =
                [this, self](const boost::system::error_code & error)
            {

                if (true == m_stopped)
                {
                    LOG_DEBUG << "matrix_server_socket_channel_handler has been stopped and shake_hand_timer_handler exit directly" << m_sid.to_string();
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

                    LOG_ERROR << "matrix server socket channel handler timer error: " << error.value() << " " << error.message() << m_sid.to_string();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return;
                }

                //lost shake hand too many times
                if (++m_lost_shake_hand_count >= m_lost_shake_hand_count_max)
                {
                    LOG_ERROR << "matrix server socket channel handler lost shake hand count error timers: " << m_lost_shake_hand_count;
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return;
                }

                //async wait
                m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
                m_shake_hand_timer.async_wait(m_shake_hand_timer_handler);
            };

            //wait ver req timer
            m_wait_ver_req_timer_handler =
                [this, self] (const boost::system::error_code & error)
            {
                if (true == m_stopped)
                {
                    LOG_DEBUG << "matrix_server_socket_channel_handler has been stopped and m_wait_ver_req_timer_handler exit directly" << m_sid.to_string();
                    return;
                }

                if (error)
                {
                    //aborted, maybe cancel triggered
                    if (boost::asio::error::operation_aborted == error.value())
                    {
                        LOG_DEBUG << "matrix server socket channel handler wait ver req timer aborted.";
                        return;
                    }

                    LOG_ERROR << "matrix server socket channel handler wait ver req timer error: " << error.value() << " " << error.message() << m_sid.to_string();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return;
                }

                //time out and disconnect tcp socket
                if (m_wait_ver_req_timer.expires_at() < std::chrono::steady_clock::now())
                {
                    LOG_ERROR << "matrix server socket channel handler connect successfully but no message received, " << m_sid.to_string();

                    //stop_wait_ver_req_timer();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return;
                }

                //restart timer for not time out
                start_wait_ver_req_timer();
            };

            return E_SUCCESS;

        }

        int32_t matrix_server_socket_channel_handler::stop()
        {
            //release timer
            stop_wait_ver_req_timer();
            stop_shake_hand_timer();

            m_stopped = true;

            return E_SUCCESS;
        }

        int32_t matrix_server_socket_channel_handler::on_after_msg_received(message &msg)
        {
            if (false == m_login_success)
            {
                if (VER_REQ != msg.get_name())
                {
                    LOG_ERROR << "matrix server socket channel received error message: " << msg.get_name() << " , while not login success" << msg.header.src_sid.to_string();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return E_SUCCESS;
                }
            }

            if (VER_REQ == msg.get_name())
            {
                if (true == m_login_success)
                {
                    LOG_ERROR << "matrix server socket channel received duplicated VER_REQ message" << msg.header.src_sid.to_string();
                    if (auto ch = m_channel.lock())
                    {
                        ch->on_error();
                    }
                    return E_SUCCESS;
                }
                else
                {
                    stop_wait_ver_req_timer();
                    return E_SUCCESS;
                }
            }

            //send shake hand resp in hanlder
            if (SHAKE_HAND_REQ == msg.get_name())
            {
                send_shake_hand_resp();
            }

            m_lost_shake_hand_count = 0;
            return E_SUCCESS;
        }

        int32_t matrix_server_socket_channel_handler::on_after_msg_sent(message &msg)
        {
            if (VER_RESP == msg.get_name())
            {
                m_login_success = true;
                start_shake_hand_timer();

                LOG_DEBUG << "matrix server socket channel handler start shake hand timer, " << m_sid.to_string();
                return E_SUCCESS;
            }

            //reset
            m_lost_shake_hand_count = 0;
            return E_SUCCESS;
        }

        int32_t matrix_server_socket_channel_handler::on_before_msg_receive()
        {
            start_wait_ver_req_timer();
            return E_SUCCESS;
        }

        std::shared_ptr<socket_channel_handler> matrix_server_socket_channel_handler::create(std::shared_ptr<channel> ch)
        { 
            shared_ptr<socket_channel_handler> handler(new matrix_server_socket_channel_handler(ch));
            return handler->shared_from_this();
        }

        void matrix_server_socket_channel_handler::send_shake_hand_resp()
        {
            //shake_hand_req 
            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::shake_hand_resp> req_content = std::make_shared<matrix::service_core::shake_hand_resp>();

            //header
            req_content->header.length = 0;
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = SHAKE_HAND_RESP;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;

            resp_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            resp_msg->set_name(SHAKE_HAND_RESP);

            if (auto ch = m_channel.lock())
            {
                ch->write(resp_msg);
            }
        }

        void matrix_server_socket_channel_handler::start_wait_ver_req_timer()
        {
            m_wait_ver_req_timer.expires_from_now(std::chrono::seconds(DEFAULT_WAIT_VER_REQ_INTERVAL));
            m_wait_ver_req_timer.async_wait(m_wait_ver_req_timer_handler);

            LOG_DEBUG << "matrix server socket channel handler start wait ver req timer, " << m_sid.to_string();
        }

        void matrix_server_socket_channel_handler::stop_wait_ver_req_timer()
        {
            boost::system::error_code error;

            m_wait_ver_req_timer.cancel(error);
            if (error)
            {
                LOG_ERROR << "matrix server socket channel handler cancel timer error: " << error;
            }
            else
            {
                LOG_DEBUG << "matrix server socket channel handler stop wait ver req timer, " << m_sid.to_string();
            }

            m_wait_ver_req_timer_handler = nullptr;
        }

    }

}