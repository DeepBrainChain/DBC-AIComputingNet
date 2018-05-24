#pragma once

#include "protocol.h"

using namespace matrix::core;
namespace matrix
{
    namespace service_core
    {
        class net_message
        {
        public :
            net_message(int32_t len = DEFAULT_BUF_LEN);
            uint32_t packet_len;
            union bytes
            {
                char b[4];
                uint32_t all;
            } theBytes;

            byte_buf msgstream;
            uint32_t pos = 0;
            bool in_data = false;
            bool complete() 
            {
                return (packet_len == pos);
            }

            uint32_t read_packet_len(byte_buf &in);
            uint32_t read_packet_body(byte_buf &in);

        };
    }
}