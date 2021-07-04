#ifndef DBC_NETWORK_SERVICE_MESSAGE_H
#define DBC_NETWORK_SERVICE_MESSAGE_H

#include "common.h"
#include "protocol.h"
#include "socket_id.h"

typedef void * MESSAGE_PTR;
#define COPY_MSG_HEADER(req,resp)      if(nullptr != req) {resp->header.__set_session_id( req->header.session_id  );}

namespace dbc
{
    namespace network
    {
        class inner_header
        {
        public:
            inner_header() : msg_name("unknown message"), msg_priority(0) {}

            std::string msg_name;
            uint32_t msg_priority;

            dbc::network::socket_id src_sid;
            dbc::network::socket_id dst_sid;
       };

        class message
        {
        public:
            message() = default;
            virtual ~message() = default;

            virtual std::string get_name() { return header.msg_name; }
            virtual void set_name(const std::string &name) { header.msg_name = name; }

            virtual uint32_t get_priority() { return header.msg_priority; }
            virtual void set_priority(uint32_t msg_priority) { header.msg_priority = msg_priority; }

            virtual std::shared_ptr<msg_base> get_content() { return content; }
            virtual void set_content(std::shared_ptr<msg_base> content) { this->content = content; }

            virtual uint32_t validate() const { return content->validate(); }
            virtual uint32_t read(protocol * iprot) { return content->read(iprot); }
            virtual uint32_t write(protocol * oprot) const { return content->write(oprot); }

            inner_header header;
            std::shared_ptr<msg_base> content;
        };
    }
}

#endif
