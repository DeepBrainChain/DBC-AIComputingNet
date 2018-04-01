/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_connector.hpp
* description    £ºtcp connector for nio client socket
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <memory>
#include <boost/asio.hpp>
#include "nio_loop_group.h"
#include "common.h"
#include "channel.h"
#include "socket_id.h"
#include "channel.h"
#include "handler_create_functor.h"
#include "service_message_def.h"

using namespace boost::asio;
using namespace boost::asio::ip;


#define RECONNECT_INTERVAL                     2                    //2->4->8->16->32->64...
#define MAX_RECONNECT_TIMES                 10



namespace matrix
{
    namespace core
    {

        class tcp_connector : public std::enable_shared_from_this<tcp_connector>, boost::noncopyable
        {
            using ios_ptr = typename std::shared_ptr<io_service>;
            using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

            typedef void (timer_handler_type)(const boost::system::error_code &);

        public:

            tcp_connector(nio_loop_ptr connector_group, nio_loop_ptr worker_group, const tcp::endpoint &connect_addr, handler_create_functor func);

            virtual ~tcp_connector() = default;

            virtual int32_t start();

            virtual int32_t stop();

            const tcp::endpoint &get_connect_addr() const { return m_connect_addr; }

        protected:

            void async_connect();                           //reconnect is decided by service layer

            virtual void on_connect(const boost::system::error_code& error);

            virtual void connect_notification(CLIENT_CONNECT_STATUS status);

        protected:

            socket_id m_sid;

            bool m_connected;

            int m_reconnect_times;

            nio_loop_ptr m_worker_group;

            const tcp::endpoint m_connect_addr;

            std::shared_ptr<channel> m_client_channel;

            handler_create_functor m_handler_create_func;

            steady_timer m_reconnect_timer;

            std::function<timer_handler_type> m_reconnect_timer_handler;

        };

    }

}
