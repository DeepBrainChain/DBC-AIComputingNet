/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºdbc_decoder.h
* description    £ºdbc decoder for network transport
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include "message_to_byte_encoder.h"
#include "length_field_frame_decoder.h"
#include "protocol.h"
#include "matrix_types.h"


using namespace matrix::core;


#define DEFAULT_DECODE_HEADER_LEN                   24

#define MAIN_NET                                                          0xF1E1B0A7
#define TEST_NET                                                           0XE1D1A097
#define PROTOCO_VERSION                                         0x00000001

#define MATRIX_MSG_LENGTH_FIELD_END_OFFSET              10                                //from msg frame begin to length field end is 10 bytes
#define MAX_MATRIX_MSG_LEN                                  (4 * 1024 * 1024)                    //max 4M bytes


namespace matrix
{
    namespace service_core
    {
        using decode_invoker_type = typename std::function<void (std::shared_ptr<message> &msg, std::shared_ptr<binary_protocol> &proto)>;

        class matrix_coder : public message_to_byte_encoder, public length_field_frame_decoder
        {
        public:

            matrix_coder();

        protected:

            void init_decode_invoker();

            template<typename msg_type>
            void decode_invoker(std::shared_ptr<message> &msg, std::shared_ptr<binary_protocol> &proto);

            decode_status decode_header_fields(channel_handler_context &ctx, byte_buf &in, uint32_t &msg_len, uint32_t &magic, std::string &msg_name);

            decode_status decode_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg);

        protected:

            std::map<std::string, decode_invoker_type> m_decode_invokers;

        };

    }

}

