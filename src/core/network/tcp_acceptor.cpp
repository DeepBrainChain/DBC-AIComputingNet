/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtcp_acceptor.cpp
* description    £ºtcp acceptor for nio server listening
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "tcp_acceptor.h"
#include "tcp_socket_channel.h"
#include "server.h"

namespace matrix
{
    namespace core
    {

        tcp_acceptor::tcp_acceptor(ios_ptr io_service, nio_loop_ptr worker_group, tcp::endpoint endpoint, handler_create_functor func)
            : m_endpoint(endpoint)
            , m_io_service(io_service)
            , m_worker_group(worker_group)
            , m_acceptor(*m_io_service, endpoint, true)
            , m_handler_create_func(func)
        {
        }

        int32_t tcp_acceptor::start()
        {
            boost::system::error_code error;
            m_acceptor.listen(DEFAULT_LISTEN_BACKLOG, error);
            if (error)
            {
                LOG_ERROR << "tcp acceptor start listen failed: " << error;
                return E_DEFAULT;
            }

            return create_channel();
        }

        int32_t tcp_acceptor::stop()
        {
            boost::system::error_code error;

            //cancel
            m_acceptor.cancel(error);
            if (error)
            {
                LOG_ERROR << "tcp acceptor cancel error: " << error;
            }

            //close
            m_acceptor.close(error);
            if (error)
            {
                LOG_ERROR << "tcp acceptor close error: " << error;
            }

            return E_SUCCESS;
        }

        int32_t tcp_acceptor::create_channel()
        {
            //alloc socket id number
            socket_id sid = socket_id_allocator::get_mutable_instance().alloc_server_socket_id();

            //create server tcp socket channel
            auto server_channel = std::make_shared<tcp_socket_channel>(m_worker_group->get_io_service(), sid, m_handler_create_func, DEFAULT_BUF_LEN);
            assert(server_channel != nullptr);

            //add to connection manager
            int32_t ret = CONNECTION_MANAGER->add_channel(sid, std::dynamic_pointer_cast<channel>(server_channel->shared_from_this()));
            assert(E_SUCCESS == ret);               //if not success, we should check whether socket id is duplicated

            //async accept
            m_acceptor.async_accept(server_channel->get_socket(), boost::bind(&tcp_acceptor::on_accept, shared_from_this(), std::dynamic_pointer_cast<channel>(server_channel->shared_from_this()), boost::asio::placeholders::error));

            return E_SUCCESS;
        }

        int32_t tcp_acceptor::on_accept(std::shared_ptr<channel> ch, const boost::system::error_code& error)
        {
            if (error)
            {
                LOG_ERROR << "tcp acceptor on accept call back error: " << error.value() << " " << error.message();

                //new channel
                create_channel();
                return E_DEFAULT;
            }

            //start run
            ch->start();
            tcp::endpoint ep = std::dynamic_pointer_cast<tcp_socket_channel>(ch)->get_remote_addr();
            LOG_DEBUG << "tcp acceptor accept new socket channel at ip: " << ep.address().to_string() << " port: " << ep.port();

            //new channel
            return create_channel();
        }

    }

}