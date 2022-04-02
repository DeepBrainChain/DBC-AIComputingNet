#include "tcp_connector.h"
#include "channel/tcp_socket_channel.h"
#include <boost/exception/all.hpp>
#include "log/log.h"
#include "connection_manager.h"
#include "topic_manager.h"

namespace network
{
    tcp_connector::tcp_connector(std::shared_ptr<io_service_pool> connector_group, 
        std::shared_ptr<io_service_pool> worker_group, const tcp::endpoint &connect_addr, 
        handler_create_functor func)
            : m_sid(socket_id_allocator::get_mutable_instance().alloc_client_socket_id()) 
            , m_worker_group(worker_group)
            , m_connect_addr(connect_addr)
            , m_handler_create_func(func)
            , m_reconnect_timer(*(connector_group->get_io_service()))
    {

    }

    tcp_connector::~tcp_connector()
    {
        
    }

    ERRCODE tcp_connector::start(uint32_t retry/* = MAX_RECONNECT_TIMES*/)
    {
        if (retry > MAX_RECONNECT_TIMES) {
            retry = MAX_RECONNECT_TIMES;
        }
        m_max_reconnect_times = retry;

        m_client_channel = std::make_shared<tcp_socket_channel>(m_worker_group->get_io_service(), 
            m_sid, m_handler_create_func, DEFAULT_BUF_LEN);

        async_connect();
        return ERR_SUCCESS;
    }

    ERRCODE tcp_connector::stop()
    {
        if (true == m_connected) {
            return ERR_SUCCESS;
        }
        
        boost::system::error_code error;
        m_reconnect_timer.cancel(error);
  
        auto client_channel = std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel);
        client_channel->close();
        
        return ERR_SUCCESS;
    }

    void tcp_connector::async_connect()
    {
        std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel)->get_socket().async_connect(
            m_connect_addr, boost::bind(&tcp_connector::on_connect, shared_from_this(), 
                                        boost::asio::placeholders::error));
    }

    void tcp_connector::on_connect(const boost::system::error_code& error)
    {
        if (error)
        {
            if (boost::asio::error::operation_aborted == error.value()) 
            {
                return;
            }
            
            std::ostringstream errorinfo;
            errorinfo << "error: " << error.value() << " " << error.message();
            LOG_ERROR << "connect failed: " << errorinfo.str();

            reconnect(errorinfo.str());

            return;
        }

        try
        {
            auto client_channel = std::dynamic_pointer_cast<tcp_socket_channel>(m_client_channel);
            if (client_channel != nullptr) 
                m_client_channel->start();    
        }
        catch (const boost::exception& e)
        {
            std::string errorinfo = diagnostic_information(e);
            reconnect(errorinfo);
            return;
        }
        
        int32_t ret = connection_manager::instance().add_channel(m_sid, m_client_channel);
        if (ERR_SUCCESS != ret)
        {
            LOG_ERROR << "add client_channel failed remote_addr=" << m_connect_addr;
            return;
        }

        LOG_INFO << "tcp client_channel connect successful, remote_addr=" << m_connect_addr;

        m_connected = true;
        m_reconnect_times = 0;

        connect_notify(CLIENT_CONNECT_SUCCESS); 
    }

    void tcp_connector::reconnect(const std::string errorinfo)
    {
        if (m_reconnect_times < m_max_reconnect_times)
        {
            m_reconnect_times++;

            int32_t interval = RECONNECT_INTERVAL << m_reconnect_times;

            LOG_INFO << "reconnect (" << m_reconnect_times << ") after " 
                << interval << "s, remote_addr=" << m_connect_addr;

            m_reconnect_timer.expires_from_now(std::chrono::seconds(interval));
            m_reconnect_timer.async_wait(boost::bind(&tcp_connector::on_reconnect_timer_expired,
                    shared_from_this(), boost::asio::placeholders::error));
        }
        else
        {
            LOG_ERROR << "reconnect reach threshold: " << m_reconnect_times
                << ", remote_addr=" << m_connect_addr;

            connect_notify(CLIENT_CONNECT_FAIL);
        }
    }

    void tcp_connector::connect_notify(CLIENT_CONNECT_STATUS status)
    {
        auto msg = std::make_shared<client_tcp_connect_notification>();
        msg->header.src_sid = m_sid;
        msg->ep = m_connect_addr;
        msg->status = status;

        msg->set_name(CLIENT_CONNECT_NOTIFICATION);
        auto send_msg = std::dynamic_pointer_cast<message>(msg);

        topic_manager::instance().publish<void>(msg->get_name(), send_msg);
    }

    void tcp_connector::on_reconnect_timer_expired(const boost::system::error_code& error)
    {
        if (error)
        {
            if (boost::asio::error::operation_aborted == error.value())
            {
                return;
            }

            LOG_ERROR << "reconnect error: " << error.value() << " " << error.message() 
                << ", remote_addr:" << m_connect_addr;
            
            return;
        }

        LOG_INFO << "begin to reconnect (" << m_reconnect_times << ") "
            << ", remote_addr=" << m_connect_addr;
        
        async_connect();
    }
}
