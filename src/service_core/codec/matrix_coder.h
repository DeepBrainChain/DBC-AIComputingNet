/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   dbc_decoder.h
* description    :   dbc decoder for network transport
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include <unordered_map>
#include "message_to_byte_encoder.h"
#include "length_field_frame_decoder.h"
#include "protocol.h"
#include "matrix_types.h"


using namespace matrix::core;
using namespace std::placeholders;


#define DEFAULT_DECODE_HEADER_LEN               24
#define MATRIX_MSG_MIN_READ_LENGTH              8
#define MAX_MATRIX_MSG_LEN                                 (4 * 1024 * 1024)                    //max 4M bytes
#define DECLARE_DECODE_INVOKER                       decode_invoker_type invoker;
#define BIND_DECODE_INVOKER(MSG_TYPE)       invoker = std::bind(&matrix_coder::decode_invoke<MSG_TYPE>, this, _1, _2, _3); \
                                                                                                m_binary_decode_invokers[#MSG_TYPE] = invoker;

#define DECLARE_ENCODE_INVOKER                       encode_invoker_type invoker;
#define BIND_ENCODE_INVOKER(MSG_TYPE)       invoker = std::bind(&matrix_coder::encode_invoke<MSG_TYPE>, this, _1, _2, _3, _4); \
                                                                                                m_binary_encode_invokers[#MSG_TYPE] = invoker;

#define THRIFT_BINARY_PROTO                                0


namespace matrix
{
    namespace service_core
    {
        
        //do not use virtual function
        class matrix_packet_header
        {
        public:

            int32_t packet_len;
            int32_t packet_type;
        };

        class matrix_coder : public message_to_byte_encoder, public length_field_frame_decoder
        {

            using decode_invoker_type = typename std::function<void(shared_ptr<message> &, base_header &, shared_ptr<protocol> &)>;

            using encode_invoker_type = typename std::function<void(channel_handler_context &, std::shared_ptr<protocol> &, message & , byte_buf &)>;

        public:

            matrix_coder();

            virtual ~matrix_coder() = default;

            encode_status encode(channel_handler_context &ctx, message & msg, byte_buf &out);
            decode_status recv_message(byte_buf &in);

            //decode
            decode_status decode_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg);

        protected:

            void init_decode_proto();

            void init_decode_invoker();

            void init_encode_invoker();

            std::shared_ptr<protocol> get_protocol(int32_t type);

            //decode packet header
            void decode_packet_header(byte_buf &in, matrix_packet_header &packet_header);

            void encode_packet_header(matrix_packet_header &packet_header, byte_buf &out);

            decode_status decode_service_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg, std::shared_ptr<protocol> proto);

            template<typename msg_type>
            void decode_invoke(std::shared_ptr<message> &msg, base_header &msg_header, std::shared_ptr<protocol> &proto);

            template<typename msg_type>
            void encode_invoke(channel_handler_context &ctx, std::shared_ptr<protocol> &proto, message & msg, byte_buf &out);

        protected:            

            std::unordered_map<std::string, decode_invoker_type> m_binary_decode_invokers;

            std::unordered_map<std::string, encode_invoker_type> m_binary_encode_invokers;

            std::unordered_map<int32_t, std::shared_ptr<protocol>> m_decode_protos;

        };

    }

}

