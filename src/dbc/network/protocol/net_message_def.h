#ifndef DBC_NETWORK_NET_MESSAGE_DEF_H
#define DBC_NETWORK_NET_MESSAGE_DEF_H

#include "net_message.h"
#include <boost/asio.hpp>

using namespace boost::asio::ip;

namespace network
{
    class timer_click_msg : public message
    {
    public:
        uint64_t cur_tick;
    };

    class tcp_socket_channel_error_msg : public message
    {
    public:
        tcp::endpoint ep;
    };

    enum CLIENT_CONNECT_STATUS
    {
        CLIENT_CONNECT_SUCCESS = 0,
        CLIENT_CONNECT_FAIL
    };

    class client_tcp_connect_notification : public message
    {
    public:
        tcp::endpoint ep;
        CLIENT_CONNECT_STATUS status;
    };
}

#endif
