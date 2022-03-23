#ifndef DBC_NETWORK_TCP_CONNECTOR_H
#define DBC_NETWORK_TCP_CONNECTOR_H

#include <memory>
#include <boost/asio.hpp>
#include "utils/io_service_pool.h"
#include "common/common.h"
#include "channel/channel.h"
#include "utils/socket_id.h"
#include "channel/handler_create_functor.h"
#include "protocol/net_message_def.h"

using namespace boost::asio;
using namespace boost::asio::ip;

#define RECONNECT_INTERVAL         2    //2->4->8->16->32->64...
#define MAX_RECONNECT_TIMES        2

namespace network
{
    class tcp_connector : public std::enable_shared_from_this<tcp_connector>, boost::noncopyable
    {
    public:
        tcp_connector(std::shared_ptr<io_service_pool> connector_group, std::shared_ptr<io_service_pool> worker_group,
            const tcp::endpoint &connect_addr, handler_create_functor func);

        virtual ~tcp_connector();

        virtual int32_t start(uint32_t retry = MAX_RECONNECT_TIMES);

        virtual int32_t stop();

        const tcp::endpoint &get_connect_addr() const { return m_connect_addr; }

        const socket_id &get_socket_id() const { return m_sid; }

        bool is_connected() { return m_connected; }

    protected:
        void async_connect();   //reconnect is decided by service layer

        virtual void on_connect(const boost::system::error_code& error);

        virtual void connect_notify(CLIENT_CONNECT_STATUS status);

        //modify by regulus: fix connect crash
        void reconnect(const std::string errorinfo);

        void on_reconnect_timer_expired(const boost::system::error_code& error);

    protected:
        socket_id m_sid;
        tcp::endpoint m_connect_addr;
        handler_create_functor m_handler_create_func;
        std::shared_ptr<io_service_pool> m_worker_group;
        steady_timer m_reconnect_timer;
        uint32_t m_max_reconnect_times = MAX_RECONNECT_TIMES;

        bool m_connected = false;
        uint32_t m_reconnect_times = 0;
        
        std::shared_ptr<channel> m_client_channel;
    };
}

#endif
