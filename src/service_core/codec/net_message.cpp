#include "net_message.h"
#include <algorithm>
namespace matrix
{
    namespace service_core
    {
        net_message::net_message(int32_t len)
            :packet_len(0)
            ,pos(0)
            , msgstream(len)
            , in_data(false)
        {}
        uint32_t net_message::read_packet_len(byte_buf &in)
        {
            unsigned int nRemaining = 4 - pos;
            unsigned int nCopy = (std::min)(nRemaining, in.get_valid_read_len());
            msgstream.write_to_byte_buf(in.get_read_ptr(), nCopy);
            in.read_from_byte_buf(theBytes.b + pos, nCopy);
            pos += nCopy;

            if (pos < 4)
            {
                return nCopy;
            }

            packet_len = byte_order::ntoh32(theBytes.all);
            in_data = true;
            return nCopy;
        }

        uint32_t net_message::read_packet_body(byte_buf &in)
        {
            unsigned int nRemaining = packet_len - pos;
            unsigned int nCopy = (std::min)(nRemaining, in.get_valid_read_len());
            msgstream.write_to_byte_buf(in.get_read_ptr(), nCopy);
            pos += nCopy;
            in.move_read_ptr(nCopy);
            return nCopy;
        }
    }
}