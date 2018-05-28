#include "net_message.h"
#include <algorithm>
namespace matrix
{
    namespace service_core
    {
        net_message::net_message(int32_t len)
            :m_packet_len(0)
            , m_pos(0)
            , m_msg_stream(len)
            , m_in_data(false)
        {}
        uint32_t net_message::read_packet_len(byte_buf &in)
        {
            unsigned int nRemaining = 4 - m_pos;
            unsigned int nCopy = (std::min)(nRemaining, in.get_valid_read_len());
            m_msg_stream.write_to_byte_buf(in.get_read_ptr(), nCopy);
            in.read_from_byte_buf(theBytes.b + m_pos, nCopy);
            m_pos += nCopy;

            if (m_pos < 4)
            {
                return nCopy;
            }

            m_packet_len = byte_order::ntoh32(theBytes.all);
            m_in_data = true;
            return nCopy;
        }

        uint32_t net_message::read_packet_body(byte_buf &in)
        {
            unsigned int nRemaining = m_packet_len - m_pos;
            unsigned int nCopy = (std::min)(nRemaining, in.get_valid_read_len());
            m_msg_stream.write_to_byte_buf(in.get_read_ptr(), nCopy);
            m_pos += nCopy;
            in.move_read_ptr(nCopy);
            return nCopy;
        }
    }
}