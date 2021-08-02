#ifndef DBC_NETWORK_TCP_CONNECTOR_H
#define DBC_NETWORK_TCP_CONNECTOR_H

#include <memory>
#include <boost/asio.hpp>
#include "nio_loop_group.h"
#include "common/common.h"
#include "channel.h"
#include "socket_id.h"
#include "handler_create_functor.h"
#include "protocol/service_message_def.h"

using namespace boost::asio;
using namespace boost::asio::ip;

#define RECONNECT_INTERVAL                     2                    //2->4->8->16->32->64...
#define MAX_RECONNECT_TIMES                  2

namespace dbc
{
    namespace network
    {
        class tcp_connector : public std::enable_shared_from_this<tcp_connector>, boost::noncopyable
        {
            using ios_ptr = typename std::shared_ptr<io_service>;
            using nio_loop_ptr = typename std::shared_ptr<nio_loop_group>;

            typedef void (timer_handler_type)(const boost::system::error_code &);

        public:

            tcp_connector(nio_loop_ptr connector_group, nio_loop_ptr worker_group, const tcp::endpoint &connect_addr, handler_create_functor func);

            virtual ~tcp_connector();

            virtual int32_t start(uint32_t retry = MAX_RECONNECT_TIMES);

            virtual int32_t stop();

            const tcp::endpoint &get_connect_addr() const { return m_connect_addr; }

            const socket_id &get_socket_id() const { return m_sid; }

            bool is_connected() { return m_connected; }

        protected:

            void async_connect();                           //reconnect is decided by service layer

            virtual void on_connect(const boost::system::error_code& error);

            virtual void connect_notification(CLIENT_CONNECT_STATUS status);
            //modify by regulus: fix connect crash
            void reconnect(const std::string errorinfo);

            void on_reconnect_timer_expired(const boost::system::error_code& error);

        protected:

            socket_id m_sid;

            bool m_connected;

            uint32_t m_reconnect_times;

            uint32_t m_req_reconnect_times;

            nio_loop_ptr m_worker_group;

            const tcp::endpoint m_connect_addr;

            std::shared_ptr<channel> m_client_channel;

            handler_create_functor m_handler_create_func;

            steady_timer m_reconnect_timer;
        };
    }
}

#endif
