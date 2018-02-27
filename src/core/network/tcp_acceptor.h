/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_acceptor.h
* description    £ºtcp acceptor for nio server listening
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <memory>
#include <boost/asio.hpp>
#include "nio_loop_group.h"
#include "channel.h"
#include "log.h"
#include "socket_id.h"
#include "handler_create_functor.h"


using namespace boost::asio;
using namespace boost::asio::ip;


#define DEFAULT_LISTEN_BACKLOG                          8                               //default listen backlog


namespace matrix
{
    namespace core
    {

        class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor>, boost::noncopyable
        {

            using ios_ptr = typename std::shared_ptr<io_service>;
            using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

        public:

            tcp_acceptor(ios_ptr io_service, nio_loop_ptr worker_group, tcp::endpoint endpoint, handler_create_functor func);

            virtual ~tcp_acceptor() = default;

            virtual int32_t start();

            virtual int32_t stop();

            tcp::endpoint get_endpoint() const { return m_endpoint; }

        protected:

            virtual int32_t create_channel();

            virtual int32_t on_accept(std::shared_ptr<channel> ch, const boost::system::error_code& error);

        protected:

            tcp::endpoint m_endpoint;

            ios_ptr m_io_service;

            nio_loop_ptr m_worker_group;

            tcp::acceptor m_acceptor;

            handler_create_functor m_handler_create_func;

        };

    }

}
