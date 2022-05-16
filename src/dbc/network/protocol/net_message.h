#ifndef DBC_NETWORK_NET_MESSAGE_H
#define DBC_NETWORK_NET_MESSAGE_H

#include "../utils/socket_id.h"
#include "protocol.h"

typedef void * MESSAGE_PTR;

#define COPY_MSG_HEADER(req,resp) \
    if(nullptr != req) { \
        resp->header.__set_session_id(req->header.session_id); \
    }

namespace network
{
    class message_header
    {
    public:
        std::string msg_name = "unknown message";
        uint32_t msg_priority = 0;
        socket_id src_sid;
        socket_id dst_sid;
    };

    class message
    {
    public:
        message() = default;

        virtual ~message() = default;

        std::string get_name() { return header.msg_name; }
        void set_name(const std::string &name) { header.msg_name = name; }

        uint32_t get_priority() { return header.msg_priority; }
        void set_priority(uint32_t msg_priority) { header.msg_priority = msg_priority; }

        std::shared_ptr<msg_base> get_content() { return content; }
        void set_content(std::shared_ptr<msg_base> content) { this->content = content; }

        uint32_t validate() const { return content->validate(); }

        uint32_t read(protocol* iprot) { return content->read(iprot); }

        uint32_t write(protocol* oprot) const { return content->write(oprot); }

    public:
        message_header header;
        std::shared_ptr<msg_base> content;
    };
}

#endif
