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
#include <boost/exception/all.hpp>


namespace matrix
{
    namespace core
    {

        tcp_connector::tcp_connector(nio_loop_ptr connector_group, nio_loop_ptr worker_group, const tcp::endpoint &connect_addr, handler_create_functor func)
                : m_connected(false)
                , m_reconnect_times(0)
                , m_req_reconnect_times(0)
                , m_connect_addr(connect_addr)
                , m_worker_group(worker_group)
                , m_sid(socket_id_allocator::get_mutable_instance().alloc_client_socket_id())
                , m_handler_create_func(func)
                , m_reconnect_timer(*(connector_group->get_io_service()))
        {

        }

        int32_t tcp_connector::start(uint32_t retry/* = MAX_RECONNECT_TIMES*/)
        {
            if (retry > MAX_RECONNECT_TIMES)
            {
                retry = MAX_RECONNECT_TIMES;
            }
            m_req_reconnect_times = retry;

            auto self(shared_from_this());
            m_reconnect_timer_handler = [this, self](const boost::system::error_code & error)
            {
                if (error)
                {
                    //aborted, maybe cancel triggered
                    if (boost::asio::error::operation_aborted == error.value())
                    {
                        LOG_DEBUG << "tcp_connector reconnect timer aborted.";
                        return;
                    }

                    LOG_ERROR << "tcp_connector reconnect timer error: " << error.value() << " " << error.message() << m_sid.to_string();
                    return;
                }

                LOG_DEBUG << "tcp_connector reconnect timer expires and begin to async connect.";
                async_connect();
            };

            //create tcp socket channel and async connect
            m_client_channel = std::make_shared<tcp_socket_channel>(m_worker_group->get_io_service(), m_sid, m_handler_create_func, DEFAULT_BUF_LEN);
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

            //not connected, means is reconnecting......
            boost::system::error_code error;

            m_reconnect_timer.cancel(error);
            if (error)
            {
                LOG_ERROR << "tcp connector connect timer cancel error: " << error;
            }

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

        //add by regulus: fix connect crash
        void tcp_connector::reconnect(const std::string errorinfo)
        {
            if (m_reconnect_times < m_req_reconnect_times)
            {   
                if (!m_reconnect_timer_handler)
                {
                    LOG_ERROR << "no timer handler for reconnect.";
                    return;
                }

                //try again
                int32_t interval = RECONNECT_INTERVAL << m_reconnect_times;

                LOG_ERROR << "tcp connector on connect error, addr: " << m_connect_addr
                    << ", reconnect times: " << m_reconnect_times
                    << ", reconnect seconds: " << interval
                    << errorinfo
                    << m_sid.to_string();
                m_reconnect_times++;

                m_reconnect_timer.expires_from_now(std::chrono::seconds(interval));
                m_reconnect_timer.async_wait(m_reconnect_timer_handler);
            }
            else
            {
                LOG_ERROR << "tcp connector on connect error and stop reconnect, addr: " << m_connect_addr
                    << ", reconnect times: " << m_reconnect_times
                    << errorinfo
                    << m_sid.to_string();

                //not reconnect, just notification
                connect_notification(CLIENT_CONNECT_ERROR);
            }
        }
            
        void tcp_connector::on_connect(const boost::system::error_code& error)
        {
            if (error)
            {
                //modify by regulus: fix connect crash
                ostringstream errorinfo;
                errorinfo << ", error: " << error.value() << " " << error.message();
                reconnect(errorinfo.str());

                return;
            }

            //modify by regulus:if error isn't report and the connection isn't valid, then the system will be crash in channel->start
            try
            {
                //start to work
                if (E_SUCCESS != m_client_channel->start())
                {
                    LOG_ERROR << "tcp connector channel start work error " << m_connect_addr << m_sid.to_string();
                }
                else
                {
                    LOG_DEBUG << "tcp connector channel start work successfully " << m_connect_addr << m_sid.to_string();
                }
            }
            catch (const boost::exception & e)
            {
                std::string errorinfo = diagnostic_information(e);
                reconnect(errorinfo);
                return;
            }
            ////modify by regulus:add_channel after channer start
            //add to connection manager
            int32_t ret = CONNECTION_MANAGER->add_channel(m_sid, m_client_channel);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "tcp connector on connect error, add channel failed";
                return;
            }
                
            //modify by regulus: fix "m_socket_handler =nullptr" when tcp_socket_channel::write.  m_client_channel->start() before connect_notification
            m_connected = true;//mark it before publish                       
            m_reconnect_times = 0;//reset
            connect_notification(CLIENT_CONNECT_SUCCESS);//last sentence            
        }

        void tcp_connector::connect_notification(CLIENT_CONNECT_STATUS status)
        {
            auto msg = std::make_shared<client_tcp_connect_notification>();

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