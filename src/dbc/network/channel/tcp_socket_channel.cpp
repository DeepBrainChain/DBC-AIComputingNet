#include "tcp_socket_channel.h"
#include <boost/exception/all.hpp>
#include "network/protocol/net_message_def.h"
#include <boost/bind.hpp>
#include "server/server.h"
#include "../topic_manager.h"

namespace network
{
    tcp_socket_channel::tcp_socket_channel(std::shared_ptr<io_service> ioservice, socket_id sid, 
        handler_create_functor func, int32_t len)
        : m_ioservice(ioservice)
        , m_sid(sid)
        , m_recv_buf(len)
        , m_send_buf(new byte_buf(DEFAULT_BUF_LEN))
        , m_socket(*ioservice)
        , m_handler_functor(func)
    {

    }

    tcp_socket_channel::~tcp_socket_channel()
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_send_queue.clear();
    }

    int32_t tcp_socket_channel::start()
    {
        init_socket_option();

        m_remote_addr = m_socket.remote_endpoint();
		m_local_addr = m_socket.local_endpoint();

        m_socket_handler = m_handler_functor(shared_from_this());
        m_socket_handler->start();
        m_socket_handler->on_before_msg_receive();

        m_state = CHANNEL_ACTIVE; 

        return this->read();
    }

    void tcp_socket_channel::init_socket_option()
    {
        socket_base::send_buffer_size send_buf_size_option(DEFAULT_TCP_SOCKET_SEND_BUF_LEN);
        m_socket.set_option(send_buf_size_option);

        socket_base::receive_buffer_size recv_buf_size_option(DEFAULT_TCP_SOCKET_RECV_BUF_LEN);
        m_socket.set_option(recv_buf_size_option);

        m_socket.non_blocking(true);
        m_socket.set_option(tcp::no_delay(true));
        m_socket.set_option(socket_base::keep_alive(true));
    }

    bool tcp_socket_channel::close()
    {
        m_state = CHANNEL_STOPPED;

        boost::system::error_code error;
        m_socket.cancel(error);
        m_socket.close(error);

        return ERR_SUCCESS;
    }

    int32_t tcp_socket_channel::stop()
    {
        m_ioservice->post([this]() {
            if (m_socket_handler)
            {
                m_socket_handler->stop();
            }

            boost::system::error_code error;
            m_socket.cancel(error);
            m_socket.close(error);

            m_state = CHANNEL_STOPPED;
        });
      
        return ERR_SUCCESS;
    }

    int32_t tcp_socket_channel::read()
    {
        async_read();
        return ERR_SUCCESS;
    }

    void tcp_socket_channel::async_read()
    {
        if (CHANNEL_STOPPED == m_state) 
            return;

        m_recv_buf.move_buf();
        
        if (0 == m_recv_buf.get_valid_write_len()) {
            on_error();
            return;
        }

        m_socket.async_read_some(boost::asio::buffer(m_recv_buf.get_write_ptr(), m_recv_buf.get_valid_write_len()),
            boost::bind(&tcp_socket_channel::on_read, shared_from_this(), boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred));
    }

    void tcp_socket_channel::on_read(const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (CHANNEL_STOPPED == m_state) 
            return;

        if (error)
        {
            if (boost::asio::error::operation_aborted == error.value())
            {
                return;
            }

            // EREMOTEIO Remote I/O error
            if (121 == error.value())
            {
                async_read();
                return;
            }

            //other error
            LOG_INFO << "on_read error: " << error.value() << ":" << error.message() 
                << ", sid=" << m_sid.to_string() 
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();

            on_error();
            return;
        }

        //no bytes read
        if (0 == bytes_transferred)
        {
            async_read();
            return;
        }

        try
        {
            if (ERR_SUCCESS != m_recv_buf.move_write_ptr((uint32_t)bytes_transferred))
            {
                on_error();
                return;
            }
            
            //decode
            channel_handler_context handler_context;
            if (ERR_SUCCESS == m_socket_handler->on_read(handler_context, m_recv_buf))
            {
                async_read();
            }
            else
            {
                LOG_INFO << "socket_channel_handler on_read error: "
                    << "sid=" << m_sid.to_string()
                    << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
                on_error();
            }
        }
        catch (const std::exception & e)
        {
            LOG_ERROR << "socket_channel_handler on_read exception: " << e.what()
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "socket_channel_handler on_read exception: " << diagnostic_information(e) 
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
        }
        catch (...)
        {
            LOG_ERROR << "socket_channel_handler on_read exception: "
                << " sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
        }
    }

    int32_t tcp_socket_channel::write(std::shared_ptr<message> msg)
    {
        if (CHANNEL_STOPPED == m_state) 
            return ERR_ERROR;

        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_send_queue.size() > MAX_SEND_QUEUE_MSG_COUNT)
            {
                LOG_INFO << "send queue is full"
                    << ", sid=" << m_sid.to_string()
                    << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
                return ERR_ERROR;
            }
        }

        try
        {
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                if (!m_send_queue.empty())
                {
                    m_send_queue.push_back(msg);
                    return ERR_SUCCESS;
                }

                m_send_queue.push_back(msg);
            }
            
            if (m_send_buf->get_valid_read_len() > 0)
            {
                m_send_buf->reset(); //queue is empty means send buf has been sent completely
            }

            //encode
            channel_handler_context handler_context;
            if (ERR_SUCCESS != m_socket_handler->on_write(handler_context, *msg, *m_send_buf))
            {
                LOG_ERROR << "socket_channel_handler on_write error: " 
                    << " sid=" << m_sid.to_string()
                    << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
                on_error();
                return ERR_ERROR;
            }
            
            async_write(m_send_buf);
        }
        catch (const std::exception & e)
        {
            LOG_ERROR << "socket_channel_handler on_write exception: " << e.what() 
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
            return ERR_ERROR;
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "socket_channel_handler on_write exception: " << diagnostic_information(e) 
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
            return ERR_ERROR;
        }
        catch (...)
        {
            LOG_ERROR << "socket_channel_handler on_write exception: " 
                << " sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
            return ERR_ERROR;
        }

        return ERR_SUCCESS;
    }

    void tcp_socket_channel::async_write(std::shared_ptr<byte_buf> &msg_buf)
    {
        if (CHANNEL_STOPPED == m_state) 
            return;
        
        m_socket.async_write_some(boost::asio::buffer(msg_buf->get_read_ptr(), msg_buf->get_valid_read_len()),
            boost::bind(&tcp_socket_channel::on_write, shared_from_this(), boost::asio::placeholders::error, 
                        boost::asio::placeholders::bytes_transferred));
    }

    void tcp_socket_channel::on_write(const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (CHANNEL_STOPPED == m_state) 
            return;

        if (error)
        {
            if (boost::asio::error::operation_aborted == error.value())
            {
                return;
            }

            //other error
            LOG_INFO << "on_write error: " << error.value() << ":" << error.message()
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();

            on_error();
            return;
        }

        try
        {
            //no bytes sent
            if (0 == bytes_transferred)
            {
                async_write(m_send_buf);
            }
            //not all bytes sent 
            else if (bytes_transferred < m_send_buf->get_valid_read_len())
            {
                m_send_buf->move_read_ptr((uint32_t)bytes_transferred);
                async_write(m_send_buf);
            }
            //all sent
            else if (bytes_transferred == m_send_buf->get_valid_read_len())
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_send_buf->reset();

                //callback
                std::shared_ptr<message> msg = m_send_queue.front();
                m_socket_handler->on_after_msg_sent(*msg);
                
                msg = nullptr; 
                m_send_queue.pop_front();

                //send next msg
                if (0 != m_send_queue.size())
                {
                    msg = m_send_queue.front();
                    channel_handler_context handler_context;
                    if (ERR_SUCCESS != m_socket_handler->on_write(handler_context, *msg, *m_send_buf))
                    {
                        LOG_ERROR << "socket_channel_handler on_write error: "
                            << " sid=" << m_sid.to_string()
                            << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
                        on_error();
                        return;
                    }
                    
                    async_write(m_send_buf);
                }
            }
            //larger than valid read bytes
            else
            {
                LOG_ERROR << "on_write error, bytes transferred: " << bytes_transferred 
                    << " larger than valid read_len: " << m_send_buf->get_valid_read_len()
                    << ", sid=" << m_sid.to_string()
                    << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
                on_error();
            }
        }
        catch (const std::exception & e)
        {
            LOG_ERROR << "on_write exception: " << e.what()
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port(); 
            on_error();
        }
        catch (const boost::exception & e)
        {
            LOG_ERROR << "socket_channel_handler on_write exception: " << diagnostic_information(e)
                << ", sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port(); 
            on_error();
        }
        catch (...)
        {
            LOG_ERROR << "socket_channel_handler on_write exception: "
                << " sid=" << m_sid.to_string()
                << ", remote=" << get_remote_addr().address().to_string() << ":" << get_remote_addr().port();
            on_error();
        }
    }

    void tcp_socket_channel::on_error()
    {
        error_notify();
    }

    void tcp_socket_channel::error_notify()
    {
        auto error_msg = std::make_shared<tcp_socket_channel_error_msg>();
        error_msg->header.src_sid = this->m_sid;
        error_msg->set_name(TCP_CHANNEL_ERROR);
        error_msg->ep = m_remote_addr;

        std::shared_ptr<message> msg = std::dynamic_pointer_cast<message>(error_msg);

        topic_manager::instance().publish<void>(msg->get_name(), msg);
    }

    bool tcp_socket_channel::is_channel_ready()
    {
        if (nullptr == m_socket_handler || !m_socket_handler->is_logined()
            || CHANNEL_STOPPED == m_state)
        {
            return false;
        }

        return true;
    }

    void tcp_socket_channel::set_proto_capacity(std::string c)
    {
        m_proto_capacity.add(c);
    }

    matrix_capacity& tcp_socket_channel::get_proto_capacity()
    {
        return m_proto_capacity;
    }
}
