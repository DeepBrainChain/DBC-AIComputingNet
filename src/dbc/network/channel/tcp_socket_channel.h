#ifndef DBC_NETWORK_TCP_SOCKET_CHANNEL_H
#define DBC_NETWORK_TCP_SOCKET_CHANNEL_H

#include "common/common.h"
#include <memory>
#include <boost/asio.hpp>
#include "channel.h"
#include "socket_channel_handler.h"
#include "handler_create_functor.h"
#include "network/compress/matrix_capacity.h"

using namespace boost::asio;
using namespace boost::asio::ip;

#define DEFAULT_TCP_SOCKET_SEND_BUF_LEN     (32 * 1024)
#define DEFAULT_TCP_SOCKET_RECV_BUF_LEN     (32 * 1024)

#define TCP_CHANNEL_ERROR                   "tcp_socket_channel_error"

namespace network
{
    class tcp_socket_channel 
        : public channel, public std::enable_shared_from_this<tcp_socket_channel>, public boost::noncopyable
    {
    public:
        tcp_socket_channel(std::shared_ptr<io_service> ioservice, socket_id sid, 
            handler_create_functor func, int32_t len = DEFAULT_BUF_LEN);

        virtual ~tcp_socket_channel();

        tcp::socket& get_socket() { return m_socket; }
        
        channel_type get_type() override { return tcp_channel; }

        socket_id id() override { return m_sid; }

        io_service *get_io_service() override { return m_ioservice.get(); }

        int32_t start() override;

        int32_t stop() override;

        int32_t read() override;

        int32_t write(std::shared_ptr<message> msg) override;

        bool close() override;

        void on_error() override;

        //login success and not stopped, channel is ok to send normal service message
        bool is_channel_ready() override;

        bool is_stopped() override { return m_state == CHANNEL_STOPPED; }
         
        channel_state get_state() override { return m_state; }

        tcp::endpoint get_remote_addr() const { return m_remote_addr; }

        tcp::endpoint get_local_addr() const { return m_local_addr; }

        void set_remote_node_id(std::string node_id) { m_remote_node_id = node_id; }

        std::string get_remote_node_id() { return m_remote_node_id; }

        void set_proto_capacity(std::string c);

        matrix_capacity& get_proto_capacity();

    protected:
        void init_socket_option();

        void async_read();

        virtual void on_read(const boost::system::error_code& error, size_t bytes_transferred);

        void async_write(std::shared_ptr<byte_buf> &msg_buf);

        virtual void on_write(const boost::system::error_code& error, size_t bytes_transferred);

        virtual void error_notify();
            
    protected:
        channel_state m_state = CHANNEL_ACTIVE;
        
        std::shared_ptr<io_service> m_ioservice;
        socket_id m_sid;
        handler_create_functor m_handler_functor;

        byte_buf m_recv_buf;
        std::shared_ptr<socket_channel_handler> m_socket_handler;

        std::mutex m_queue_mutex;
        std::list<std::shared_ptr<message>> m_send_queue;
        std::shared_ptr<byte_buf> m_send_buf;

        tcp::socket m_socket;
        tcp::endpoint m_remote_addr;
        tcp::endpoint m_local_addr;

        std::string m_remote_node_id; //used for efficient query response transport
        matrix_capacity m_proto_capacity; //for protocol selection
    };
}

#endif
