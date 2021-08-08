#include "tcp_acceptor.h"
#include "tcp_socket_channel.h"
#include <boost/exception/all.hpp>
#include "server/server.h"
#include "connection_manager.h"

namespace dbc
{
    namespace network
    {

        tcp_acceptor::tcp_acceptor(ios_weak_ptr io_service, nio_loop_ptr worker_group, tcp::endpoint endpoint, handler_create_functor func)
            : m_endpoint(endpoint)
            , m_io_service(io_service)
            , m_worker_group(worker_group)
            , m_acceptor(*io_service.lock(), endpoint, true)
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

            LOG_DEBUG << "channel create_channel use count " << server_channel.use_count() << server_channel->id().to_string();
            //LOG_DEBUG << "channel create_channel add_channel end use count " << server_channel.use_count() << server_channel->id().to_string();

            //async accept
            assert(nullptr != server_channel->shared_from_this());
            m_acceptor.async_accept(server_channel->get_socket(), boost::bind(&tcp_acceptor::on_accept, shared_from_this(), std::dynamic_pointer_cast<channel>(server_channel->shared_from_this()), boost::asio::placeholders::error));
            LOG_DEBUG << "channel async_accept end use count " << server_channel.use_count() << server_channel->id().to_string();

            return E_SUCCESS;
        }

        int32_t tcp_acceptor::on_accept(std::shared_ptr<channel> ch, const boost::system::error_code& error)
        {
            std::shared_ptr<tcp_socket_channel> socket_channel =  std::dynamic_pointer_cast<tcp_socket_channel>(ch);

            if (nullptr == ch || nullptr == socket_channel)
            {
                LOG_ERROR << "tcp acceptor on acceptor nullptr and exit";
                return E_DEFAULT;
            }

            if (error)
            {
                //aborted, maybe cancel triggered
                if (boost::asio::error::operation_aborted == error.value())
                {
                    LOG_DEBUG << "tcp acceptor on accept aborted: " << error.value() << " " << error.message();
                    return E_DEFAULT;
                }

                LOG_ERROR << "tcp acceptor on accept call back error: " << error.value() << " " << error.message();

                socket_channel->on_error();

                create_channel();

                return E_DEFAULT;
            }
			
            try
            {
                //start run
                socket_channel->start();
                tcp::endpoint ep = std::dynamic_pointer_cast<tcp_socket_channel>(socket_channel)->get_remote_addr();
                LOG_DEBUG << "tcp acceptor accept new socket channel at ip: " << ep.address().to_string() << " port: " << ep.port() << socket_channel->id().to_string();

            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "tcp acceptor on accept call back error: " << diagnostic_information(e);

                socket_channel->on_error();
                create_channel();
                return E_DEFAULT;
            }
			
            //add to connection manager
            int32_t ret = connection_manager::instance().add_channel(socket_channel->id(), socket_channel->shared_from_this());
            //assert(E_SUCCESS == ret);               //if not success, we should check whether socket id is duplicated
            if (ret != E_SUCCESS)
            {
                LOG_DEBUG << "tcp acceptor abnormal." << ch->id().to_string();
                ch->close();
                ch->on_error();
            }
            LOG_DEBUG << "tcp acceptor add channel to connection manager" << socket_channel->id().to_string() << " remote addr:" << socket_channel->get_remote_addr();

            //new channel
            return create_channel();
        }

    }

}