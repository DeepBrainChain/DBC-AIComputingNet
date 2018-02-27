/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_socket_channel.h
* description    £ºtcp socket channel for nio socket transport
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <memory>
#include <boost/asio.hpp>
#include "common.h"
#include "channel.h"
#include "byte_buf.h"
#include "server.h"
#include "service_message.h"
#include "service_message_def.h"

#include "socket_channel_handler.h"
#include "channel_handler_context.h"


using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

#define DEFAULT_TCP_SOCKET_SEND_BUF_LEN                  (32 * 1000)
#define DEFAULT_TCP_SOCKET_RECV_BUF_LEN                  (32 * 1000)


namespace matrix
{
    namespace core
    {

        class tcp_socket_channel : public channel, public std::enable_shared_from_this<tcp_socket_channel>, public boost::noncopyable
        {
        public:

            using ios_ptr = typename std::shared_ptr<io_service>;

            tcp_socket_channel(ios_ptr ios, socket_id sid, handler_create_functor func, int32_t len = DEFAULT_BUF_LEN);

            virtual ~tcp_socket_channel();

            tcp::socket & get_socket() { return m_socket; }

            virtual int32_t start();

            virtual int32_t stop();

            virtual int32_t read();

            virtual int32_t write(std::shared_ptr<message> msg);

            virtual void on_error();

            socket_id id() { return m_sid; }

            tcp::endpoint get_remote_addr() const { return m_remote_addr; }

            io_service *get_io_service() { return m_ios.get(); }

        protected:

            void init_option();

            void async_read();

            virtual void on_read(const boost::system::error_code& error, size_t bytes_transferred);

            void async_write(std::shared_ptr<byte_buf> &msg_buf);

            virtual void on_write(const boost::system::error_code& error, size_t bytes_transferred);

            virtual void error_notify();

            void release();
            
        protected:

            ios_ptr m_ios;

            socket_id m_sid;

            byte_buf m_recv_buf;

            std::mutex m_queue_mutex;

            std::list<std::shared_ptr<message>> m_send_queue;

            std::shared_ptr<byte_buf> m_send_buf;

            std::shared_ptr<socket_channel_handler> m_socket_handler;

            channel_handler_context m_handler_context;

            tcp::socket m_socket;

            tcp::endpoint m_remote_addr;

        };

    }

}
