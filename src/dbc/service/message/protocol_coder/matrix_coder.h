#ifndef DBC_MATRIX_CODER_H
#define DBC_MATRIX_CODER_H

#include <unordered_map>
#include "message_to_byte_encoder.h"
#include "length_field_frame_decoder.h"
#include "protocol.h"
#include "matrix_types.h"

using namespace matrix::core;
using namespace std::placeholders;

#define DEFAULT_DECODE_HEADER_LEN               24
#define MATRIX_MSG_MIN_READ_LENGTH              8
#define MAX_MATRIX_MSG_LEN                      (4 * 1024 * 1024)   //max 4M bytes

#define DECLARE_DECODE_INVOKER             decode_invoker_type invoker;
#define BIND_DECODE_INVOKER(MSG_TYPE)   \
    invoker = std::bind(&matrix_coder::decode_invoke<MSG_TYPE>, this, _1, _2, _3); \
    m_binary_decode_invokers[#MSG_TYPE] = invoker;

#define DECLARE_ENCODE_INVOKER             encode_invoker_type invoker;
#define BIND_ENCODE_INVOKER(MSG_TYPE)   \
    invoker = std::bind(&matrix_coder::encode_invoke<MSG_TYPE>, this, _1, _2, _3, _4); \
    m_binary_encode_invokers[#MSG_TYPE] = invoker;

namespace matrix {
    namespace service_core {
        //do not use virtual function
        class matrix_packet_header {
        public:
            matrix_packet_header();

            void read(byte_buf &in);

            bool update(byte_buf &out);

            void write(byte_buf &out);

            int32_t packet_len;
            int32_t packet_type;
        };

        class matrix_coder : public message_to_byte_encoder, public length_field_frame_decoder {
            using decode_invoker_type = typename std::function<void(shared_ptr<dbc::network::message> &, dbc::network::base_header &,
                                                                    shared_ptr<dbc::network::protocol> &)>;
            using encode_invoker_type = typename std::function<void(dbc::network::channel_handler_context &,
                                                                    std::shared_ptr<dbc::network::protocol> &, dbc::network::message &,
                                                                    byte_buf &)>;
        public:
            matrix_coder();

            virtual ~matrix_coder() = default;

            encode_status encode(dbc::network::channel_handler_context &ctx, dbc::network::message &msg, byte_buf &out) override;

            decode_status recv_message(byte_buf &in) override;

            decode_status decode_frame(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &msg) override;

        protected:
            void init_decode_proto();

            void init_decode_invoker();

            void init_encode_invoker();

            std::shared_ptr<dbc::network::protocol> get_protocol(int32_t type);

            decode_status
            decode_service_frame(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &msg,
                                 std::shared_ptr<dbc::network::protocol> proto);

            decode_status decode_fast_forward(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &msg,
                                              dbc::network::base_header &header, std::shared_ptr<dbc::network::protocol> proto);

            template<typename msg_type>
            void
            decode_invoke(std::shared_ptr<dbc::network::message> &msg, dbc::network::base_header &msg_header, std::shared_ptr<dbc::network::protocol> &proto);

            template<typename msg_type>
            void
            encode_invoke(dbc::network::channel_handler_context &ctx, std::shared_ptr<dbc::network::protocol> &proto, dbc::network::message &msg, byte_buf &out);

            bool get_compress_enabled(dbc::network::channel_handler_context &ctx);

            int get_thrift_proto(dbc::network::channel_handler_context &ctx);

            std::string get_local_node_id(dbc::network::channel_handler_context &ctx);

        protected:
            std::unordered_map<std::string, decode_invoker_type> m_binary_decode_invokers;

            std::unordered_map<std::string, encode_invoker_type> m_binary_encode_invokers;

            std::unordered_map<int32_t, std::shared_ptr<dbc::network::protocol>> m_decode_protos;
        };
    }
}

#endif
