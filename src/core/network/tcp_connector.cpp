/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_connector.cpp
* description    £ºtcp connector for nio client socket
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "tcp_connector.h"
#include "tcp_socket_channel.h"
#include "server.h"
#include "service_message_id.h"


#pragma warning(disable : 4996)

namespace matrix
{
    namespace core
    {

        tcp_connector::tcp_connector(nio_loop_ptr worker_group, const tcp::endpoint &connect_addr, handler_create_functor func)
                : m_connected(false)
                , m_reconnect_times(0)
                , m_connect_addr(connect_addr)
                , m_worker_group(worker_group)
                , m_sid(socket_id_allocator::get_mutable_instance().alloc_client_socket_id())
                , m_handler_create_func(func)
            {                
            }

            int32_t tcp_connector::start()
            {
                //create tcp socket channel and async connect
                m_client_channel = std::dynamic_pointer_cast<channel>(std::make_shared<tcp_socket_channel>(m_worker_group->get_io_service(), m_sid, m_handler_create_func, DEFAULT_BUF_LEN));
                async_connect();
                return E_SUCCESS;
            }

            int32_t tcp_connector::stop()
            {
                if (true == m_connected)
                {
                    LOG_DEBUG << "tcp connector stop: has connected to remote server and return";
                    return E_SUCCESS;
                }

                boost::system::error_code error;

                //cancel
                std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel)->get_socket().cancel(error);
                if (error)
                {
                    LOG_ERROR << "tcp connector cancel error: " << error;
                }

                //close
                std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel)->get_socket().close(error);
                if (error)
                {
                    LOG_ERROR << "tcp connector close error: " << error;
                }

                return E_SUCCESS;
            }

            void tcp_connector::async_connect()
            {
                std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel)->get_socket().async_connect(m_connect_addr, boost::bind(&tcp_connector::on_connect, shared_from_this(), boost::asio::placeholders::error));
            }

            void tcp_connector::on_connect(const boost::system::error_code& error)
            {
                if (error)
                {
                    LOG_ERROR << "tcp connector on connect error, reconnect times: " << ++m_reconnect_times << ", " << m_sid.to_string() << " ,error code: " << error.message();

                    if (m_reconnect_times < MAX_RECONNECT_TIMES)
                    {
                        //try again
                        std::this_thread::sleep_for(3s);
                        async_connect();
                    }
                    else
                    {
                        connect_notification(CLIENT_CONNECT_ERROR);
                    }

                    return;
                }

                m_connected = true;

                //reset
                m_reconnect_times = 0;

                //add to connection manager
                int32_t ret = CONNECTION_MANAGER->add_channel(m_sid, m_client_channel);
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "tcp connector on connect error, add channel failed";
                    return;
                }

                //publish
                connect_notification(CLIENT_CONNECT_SUCCESS);

                //start to work
                if (E_SUCCESS != m_client_channel->start())
                {
                    LOG_ERROR << "tcp connector channel start work error " << m_sid.to_string();
                }
                else
                {
                    LOG_DEBUG << "tcp connector channel start work successfully " << m_sid.to_string();
                }
            }

            void tcp_connector::connect_notification(CLIENT_CONNECT_STATUS status)
            {
                auto msg = std::make_shared<client_tcp_connect_notification>();

                msg->set_name(CLIENT_CONNECT_NOTIFICATION);
                msg->header.src_sid = this->m_sid;
                msg->ep = std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel)->get_remote_addr();
                msg->status = status;

                msg->set_name(CLIENT_CONNECT_NOTIFICATION);
                auto send_msg = std::dynamic_pointer_cast<message>(msg);

                //notify this to service layer
                TOPIC_MANAGER->publish<int32_t>(msg->get_name(), send_msg);
            }

    }

}