#ifndef DBC_NETWORK_CHANNEL_H
#define DBC_NETWORK_CHANNEL_H

#include <memory>
#include <boost/asio.hpp>
#include "protocol/service_message.h"

#define MAX_SEND_QUEUE_MSG_COUNT            204800

using namespace boost::asio;

namespace dbc
{
    namespace network
    {
        enum channel_type
        {
            tcp_channel = 0,
            upd_channel,
            http_channel
        };

        enum channel_state
        {
            CHANNEL_ACTIVE,
            CHANNEL_STOPPED            
        };

        class channel
        {
        public:

            virtual ~channel() = default;

            virtual int32_t start() = 0;

            virtual int32_t stop() = 0;

            virtual int32_t read() = 0;

            virtual int32_t write(std::shared_ptr<message> msg) = 0;

            virtual void on_error() = 0;

            virtual socket_id id() = 0;

            virtual io_service *get_io_service() = 0;

            virtual channel_type get_type() = 0;

            virtual bool is_channel_ready() = 0;

            virtual channel_state get_state() = 0;

            virtual bool is_stopped() = 0;
            virtual bool close() = 0;
        };
    }
}

#endif
