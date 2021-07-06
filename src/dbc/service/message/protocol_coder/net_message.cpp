#include "net_message.h"
#include <algorithm>

net_message::net_message(int32_t len)
        : m_packet_len(0), m_pos(0), m_msg_stream(len), m_in_data(false) {}

int32_t net_message::read_packet_len(byte_buf &in) {
    int32_t nRemaining = 4 - m_pos;
    int32_t nCopy = (std::min)(nRemaining, (int32_t) in.get_valid_read_len());
    m_msg_stream.write_to_byte_buf(in.get_read_ptr(), nCopy);
    in.read_from_byte_buf(theBytes.b + m_pos, nCopy);
    m_pos += nCopy;

    if (m_pos < 4) {
        return nCopy;
    }

    m_packet_len = dbc::network::byte_order::ntoh32(theBytes.all);
    m_in_data = true;
    return nCopy;
}

int32_t net_message::read_packet_body(byte_buf &in) {
    int32_t nRemaining = m_packet_len - m_pos;
    int32_t nCopy = (std::min)(nRemaining, (int32_t) in.get_valid_read_len());
    m_msg_stream.write_to_byte_buf(in.get_read_ptr(), nCopy);
    m_pos += nCopy;
    in.move_read_ptr(nCopy);
    return nCopy;
}
