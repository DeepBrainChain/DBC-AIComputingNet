#ifndef DBC_NETWORK_MATRIX_COMPRESS_H
#define DBC_NETWORK_MATRIX_COMPRESS_H

#include "memory/byte_buf.h"

using namespace matrix::core;

namespace dbc {
    namespace network {

        class matrix_compress
        {
        public:

            enum {
                PACKET_LEN_FIELD_OFFSET = 4,
                PACKET_HEADER_LEN = 8
            };

            enum {
                PACKET_LEN_FIELD_SIZE = 4,
                PACKET_PROTO_TYPE_FIELD_SIZE = 4
            };

            enum {
                NO_COMPRESS = 0,
                COMPRESS = 1
            };

            enum {
                MIN_MSG_LEN_TO_COMPRESS = 256
            };

            static bool compress(matrix::core::byte_buf &inout);

            static bool uncompress(matrix::core::byte_buf &in, matrix::core::byte_buf &out);

            static bool has_compress_flag(byte_buf& in);
        };
    }
}

#endif
