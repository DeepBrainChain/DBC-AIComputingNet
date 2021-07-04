#ifndef DBC_NETWORK_SOCKET_CHANNEL_HANDLER_H
#define DBC_NETWORK_SOCKET_CHANNEL_HANDLER_H

#include <boost/enable_shared_from_this.hpp>
#include "byte_buf.h"
#include "service_message.h"
#include "channel_handler_context.h"

namespace dbc
{
    namespace network
    {

        class socket_channel_handler : public std::enable_shared_from_this<socket_channel_handler>, public boost::noncopyable
        {
        public:

            virtual ~socket_channel_handler() {}

            virtual int32_t start() = 0;

            virtual int32_t stop() = 0;

        public:

            virtual int32_t on_read(channel_handler_context &ctx, matrix::core::byte_buf &in) = 0;

            virtual int32_t on_write(channel_handler_context &ctx, message &msg, matrix::core::byte_buf &buf) = 0;

            virtual int32_t on_error() = 0;

        public:

            virtual int32_t on_before_msg_send(message &msg) = 0;

            virtual int32_t on_after_msg_sent(message &msg) = 0;

            virtual int32_t on_before_msg_receive() = 0;

            virtual int32_t on_after_msg_received(message &msg) = 0;

            virtual bool is_logined() = 0;

        };

    }

}

#endif
