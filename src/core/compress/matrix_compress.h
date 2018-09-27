

#pragma once

#include "memory/byte_buf.h"

using namespace matrix::core;

namespace matrix {
    namespace core {

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
                MIN_MSG_LEN_TO_COMPRESS = 512
            };

            enum {
                UNCOMPRESS_NOK = -1,
                UNCOMPRESS_SKIP = 0,
                UNCOMPRESS_OK   = 1
            };

            static void compress(core::byte_buf &inout, bool force = false);

            static int uncompress(core::byte_buf &in, core::byte_buf &out);
        };
    }
}