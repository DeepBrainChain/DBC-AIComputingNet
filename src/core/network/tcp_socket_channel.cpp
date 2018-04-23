/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_socket_channel.cpp
* description    £ºtcp socket channel for nio socket transport
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "tcp_socket_channel.h"
#include "service_message_id.h"
#include <boost/exception/all.hpp>


namespace matrix
{
    namespace core
    {

        tcp_socket_channel::tcp_socket_channel(ios_ptr ios, socket_id sid, handler_create_functor func, int32_t len)
            : m_ios(ios)
            , m_sid(sid)
            , m_recv_buf(len)
            , m_send_buf(new byte_buf(DEFAULT_BUF_LEN))
            , m_socket(*ios)
            , m_socket_handler(func(this))
        {

        }

        tcp_socket_channel::~tcp_socket_channel()
        {
            LOG_DEBUG << "tcp socket channel destroyed, " << m_sid.to_string();
        }

        int32_t tcp_socket_channel::start()
        {
            init_option();

            //get remote addr and begin to read
            m_remote_addr = m_socket.remote_endpoint();

			m_local_addr = m_socket.local_endpoint();

            //start handler
            m_socket_handler->start();

            //callback handler
            m_socket_handler->on_before_msg_receive();

            return this->read();
        }

        int32_t tcp_socket_channel::stop()
        {
            LOG_DEBUG << "tcp_socket_channel stop: " << m_sid.to_string();

            //remove from connection manager
            CONNECTION_MANAGER->remove_channel(m_sid);

            boost::system::error_code error;

            //stop handler
            m_socket_handler->stop();

            //cancel
            m_socket.cancel(error);
            if (error)
            {
                LOG_ERROR << "tcp socket channel cancel error: " << error << m_sid.to_string();
            }

            //close
            m_socket.close(error);
            if (error)
            {
                LOG_ERROR << "tcp socket channel close error: " << error << m_sid.to_string();
            }

            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_send_queue.clear();

            return E_SUCCESS;
        }

        int32_t tcp_socket_channel::read()
        {
            async_read();
            return E_SUCCESS;
        }

        void tcp_socket_channel::init_option()
        {
            socket_base::send_buffer_size send_buf_size_option(DEFAULT_TCP_SOCKET_SEND_BUF_LEN);
            m_socket.set_option(send_buf_size_option);

            socket_base::receive_buffer_size recv_buf_size_option(DEFAULT_TCP_SOCKET_RECV_BUF_LEN);
            m_socket.set_option(recv_buf_size_option);

            m_socket.non_blocking(true);
            m_socket.set_option(tcp::no_delay(true));
            m_socket.set_option(socket_base::keep_alive(true));
        }

        void tcp_socket_channel::async_read()
        {
            //try move but not efficient
            m_recv_buf.move_buf();

            //buffer is full, it should not happen here !
            if (0 == m_recv_buf.get_valid_write_len())
            {
                //error: illegal remote socket send illegal message and server counld not decode
                LOG_ERROR << "tcp socket channel buf is zero not enough to receive socket message!" << m_sid.to_string();
                on_error();
                return;
            }

            //async read
            m_socket.async_read_some(boost::asio::buffer(m_recv_buf.get_write_ptr(), m_recv_buf.get_valid_write_len()),
                boost::bind(&tcp_socket_channel::on_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }

        void tcp_socket_channel::on_read(const boost::system::error_code& error, size_t bytes_transferred)
        {
            //read error
            if (error)
            {
                //aborted, maybe cancel triggered
                if (boost::asio::error::operation_aborted == error.value())
                {
                    LOG_DEBUG << "tcp socket channel on read aborted: " << error.value() << " " << error.message() << m_sid.to_string();
                    return;
                }

                //other error
                LOG_ERROR << "tcp socket channel on read error: " << error.value() << " "  << error.message() << m_sid.to_string();
                on_error();
                return;
            }

            //no bytes read and go!
            if (0 == bytes_transferred)
            {
                async_read();
                return;
            }

            try
            {
                //move byte_buf's write ptr location 
                if (E_SUCCESS != m_recv_buf.move_write_ptr((uint32_t)bytes_transferred))
                {
                    LOG_ERROR << "tcp socket channel move write ptr error, bytes transfered: " << bytes_transferred << m_sid.to_string();
                    on_error();
                    return;
                }

                //log
                LOG_DEBUG << "tcp socket channel " << m_sid.to_string() << " rev buf: " << m_recv_buf.to_string();

                //call back handler on_read
                if (E_SUCCESS == m_socket_handler->on_read(m_handler_context, m_recv_buf))
                {
                    //next read
                    async_read();
                }
                else
                {
                    LOG_ERROR << "tcp socket channel call socket handler on read error and ready to release socket channel" << m_sid.to_string();
                    on_error();
                }
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "tcp socket channel on read exception error " << e.what() << m_sid.to_string();
                on_error();
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "tcp socket channel on read exception error " << diagnostic_information(e) << m_sid.to_string();
                on_error();
            }
            catch (...)
            {
                LOG_ERROR << "tcp socket channel on read exception error!" << m_sid.to_string();
                on_error();
            }
        }

        int32_t tcp_socket_channel::write(std::shared_ptr<message> msg)
        {
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);

                //async_write();
                if (m_send_queue.size() > MAX_SEND_QUEUE_MSG_COUNT)
                {
                    LOG_ERROR << "tcp socket channel send queue is full" << m_sid.to_string();
                    return E_DEFAULT;
                }
            }

            try
            {
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);

                    //queue is not empty
                    if (0 != m_send_queue.size())
                    {
                        m_send_queue.push_back(msg);
                        return E_SUCCESS;
                    }

                    //queue is empty, push into queue
                    m_send_queue.push_back(msg);
                }

                //reset
                m_send_buf->reset();                     //queue is empty means send buf has been sent completely

                //encode
                if (E_SUCCESS != m_socket_handler->on_write(m_handler_context, *msg, *m_send_buf))
                {
                    LOG_ERROR << "tcp socket channel handler on write error" << m_sid.to_string();
                    on_error();
                    return E_DEFAULT;
                }

                LOG_DEBUG << "tcp socket channel " << m_sid.to_string() << " send buffer: " << m_send_buf->to_string();

                //send directly
                async_write(m_send_buf);
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "tcp socket channel write error " << e.what() << m_sid.to_string();
                on_error();

                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "tcp socket channel write error " << diagnostic_information(e) << m_sid.to_string();
                on_error();
            }
            catch (...)
            {
                LOG_ERROR << "tcp socket channel write error!" << m_sid.to_string();
                on_error();
            }

            return E_SUCCESS;
        }

        void tcp_socket_channel::async_write(std::shared_ptr<byte_buf> &msg_buf)
        {
            m_socket.async_write_some(boost::asio::buffer(msg_buf->get_read_ptr(), msg_buf->get_valid_read_len()),
                boost::bind(&tcp_socket_channel::on_write, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }

        void tcp_socket_channel::on_write(const boost::system::error_code& error, size_t bytes_transferred)
        {
            if (error)
            {
                //aborted, maybe cancel triggered
                if (boost::asio::error::operation_aborted == error.value())
                {
                    LOG_DEBUG << "tcp socket channel on write aborted: " << error.value() << " " << error.message() << m_sid.to_string();
                    return;
                }

                LOG_ERROR << "tcp socket channel on write error: " << error.value() << " " << error.message() << m_sid.to_string();
                on_error();
                return;
            }

            try
            {
                //no bytes sent
                if (0 == bytes_transferred)
                {
                    async_write(m_send_buf);                //resend
                }
                //not all bytes sent 
                else if (bytes_transferred < m_send_buf->get_valid_read_len())
                {
                    //move read ptr
                    m_send_buf->move_read_ptr((uint32_t)bytes_transferred);

                    //send left bytes
                    async_write(m_send_buf);
                }
                //well done, all sent
                else if (bytes_transferred == m_send_buf->get_valid_read_len())
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);

                    //reset
                    m_send_buf->reset();

                    //callback
                    shared_ptr<message> msg = m_send_queue.front();
                    m_socket_handler->on_after_msg_sent(*msg);

                    //pop from queue
                    m_send_queue.pop_front();

                    //send new byte_buf
                    if (0 != m_send_queue.size())
                    {
                        //encode
                        if (E_SUCCESS != m_socket_handler->on_write(m_handler_context, *msg, *m_send_buf))
                        {
                            LOG_ERROR << "tcp socket channel handler on write error" << m_sid.to_string();
                            on_error();
                            return;
                        }

                        LOG_DEBUG << "tcp socket channel " << m_sid.to_string() << " send buffer: " << m_send_buf->to_string();

                        //new message send
                        async_write(m_send_buf);
                    }
                }
                //larger than valid read bytes
                else
                {
                    LOG_ERROR << "tcp socket channel on write error, bytes_transferred: " << bytes_transferred << " larger than valid read len: " << m_send_buf->get_valid_read_len() << m_sid.to_string();
                    on_error();
                }
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "tcp socket channel on write error " << e.what() << m_sid.to_string();
                on_error();
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "tcp socket channel on write error " << diagnostic_information(e) << m_sid.to_string();
                on_error();
            }
            catch (...)
            {
                LOG_ERROR << "tcp socket channel on write error!" << m_sid.to_string();
                on_error();
            }
        }

        void tcp_socket_channel::on_error()
        {
            LOG_ERROR << "tcp socket channel error and ready to say bye!" << m_sid.to_string();
            error_notify();
            this->stop();
        }

        void tcp_socket_channel::error_notify()
        {
            auto error_msg = std::make_shared<tcp_socket_channel_error_msg>();

            error_msg->header.src_sid = this->m_sid;
            error_msg->set_name(TCP_CHANNEL_ERROR);

            std::shared_ptr<message> msg = std::dynamic_pointer_cast<message>(error_msg);

            //notify this to service layer
            TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);
        }

    }

}