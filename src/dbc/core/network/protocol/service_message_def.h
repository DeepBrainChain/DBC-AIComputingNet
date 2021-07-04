#ifndef DBC_NETWORK_SERVICE_MESSAGE_DEF_H
#define DBC_NETWORK_SERVICE_MESSAGE_DEF_H

#include "service_message.h"
#include <boost/asio.hpp>

using namespace boost::asio::ip;

namespace dbc
{
    namespace network
    {
        //timer tick msg
        class timer_click_msg : public dbc::network::message
        {
        public:
            uint64_t cur_tick;
        };

        class tcp_socket_channel_error_msg : public dbc::network::message
        {
        public:
            tcp::endpoint ep;
        };

        enum CLIENT_CONNECT_STATUS
        {
            CLIENT_CONNECT_SUCCESS      =   0,
            CLIENT_CONNECT_FAIL
        };

        class client_tcp_connect_notification : public dbc::network::message
        {
        public:
            tcp::endpoint ep;
            CLIENT_CONNECT_STATUS status;
        };

    }

}

#endif
