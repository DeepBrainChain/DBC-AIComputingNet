#pragma once

#include "protocol.h"

using namespace matrix::core;

class net_message {
public:
    net_message(int32_t len = DEFAULT_BUF_LEN);

    bool complete() {
        return (m_packet_len == m_pos);
    }

    int32_t read_packet_len(byte_buf &in);

    int32_t read_packet_body(byte_buf &in);

    byte_buf &get_message_stream() { return m_msg_stream; };

    bool has_data() { return m_in_data; };

    int32_t get_packetlen() { return m_packet_len; }

private:
    int32_t m_packet_len;
    union bytes {
        char b[4];
        int32_t all;
    } theBytes;

    int32_t m_pos = 0;
    byte_buf m_msg_stream;
    bool m_in_data = false;
};
