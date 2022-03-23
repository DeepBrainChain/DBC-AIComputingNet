#include "tcp_acceptor.h"
#include "channel/tcp_socket_channel.h"
#include <boost/exception/all.hpp>
#include "server/server.h"
#include "connection_manager.h"
 
namespace network
{
    tcp_acceptor::tcp_acceptor(std::weak_ptr<io_service> io_service, std::shared_ptr<io_service_pool> worker_group,
        tcp::endpoint endpoint, handler_create_functor func)
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
        m_acceptor.listen(8, error);
        if (error) {
            return ERR_ERROR;
        }

        return start_accept();
    }

    int32_t tcp_acceptor::stop()
    {
        boost::system::error_code error;
        m_acceptor.cancel(error); 
        m_acceptor.close(error);

        return ERR_SUCCESS;
    }

    int32_t tcp_acceptor::start_accept()
    {
        //SERVER_SOCKET
        socket_id sid = socket_id_allocator::get_mutable_instance().alloc_server_socket_id();
        auto server_channel = std::make_shared<tcp_socket_channel>(m_worker_group->get_io_service(), 
            sid, m_handler_create_func, DEFAULT_BUF_LEN);
     
        //async accept
        m_acceptor.async_accept(server_channel->get_socket(), 
            boost::bind(&tcp_acceptor::on_accept, shared_from_this(), 
                        std::dynamic_pointer_cast<channel>(server_channel->shared_from_this()), 
                        boost::asio::placeholders::error));
 
        return ERR_SUCCESS;
    }

    int32_t tcp_acceptor::on_accept(std::shared_ptr<channel> ch, const boost::system::error_code& error)
    {
        std::shared_ptr<tcp_socket_channel> socket_channel = std::dynamic_pointer_cast<tcp_socket_channel>(ch);
        if (nullptr == ch || nullptr == socket_channel)
        {
            return ERR_ERROR;
        }

        if (error)
        {
            if (boost::asio::error::operation_aborted == error.value())
            {
                return ERR_ERROR;
            }

            socket_channel->on_error();

            start_accept();

            return E_DEFAULT;
        }
			
        try
        {
            socket_channel->start();
            LOG_INFO << "accept new connection from " 
                << std::dynamic_pointer_cast<tcp_socket_channel>(socket_channel)->get_remote_addr();
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "tcp_acceptor on_accept exception: " << diagnostic_information(e);

            socket_channel->on_error();
            start_accept();
            return E_DEFAULT;
        }
	    
        int32_t ret = connection_manager::instance().add_channel(socket_channel->id(), socket_channel->shared_from_this());
        if (ret != ERR_SUCCESS) {
            LOG_ERROR << "tcp_acceptor add server_channel failed remote_addr="
                << std::dynamic_pointer_cast<tcp_socket_channel>(socket_channel)->get_remote_addr();
            ch->close();
            ch->on_error();
        }

        return start_accept();
    }
}
 