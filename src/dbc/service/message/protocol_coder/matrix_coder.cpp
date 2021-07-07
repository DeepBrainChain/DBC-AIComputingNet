#include "matrix_coder.h"
#include "compress/matrix_compress.h"
#include "service_message_id.h"
#include "thrift_compact.h"

namespace matrix {
    namespace service_core {

        matrix_coder::matrix_coder()
                : length_field_frame_decoder(MATRIX_MSG_MIN_READ_LENGTH, MAX_MATRIX_MSG_LEN) {
            init_decode_proto();

            init_decode_invoker();
            init_encode_invoker();
        }

        void matrix_coder::init_decode_proto() {
            m_decode_protos[THRIFT_BINARY_PROTO] = make_shared<dbc::network::binary_protocol>();
            m_decode_protos[THRIFT_COMPACT_PROTO] = make_shared<dbc::network::compact_protocol>();
        }

        void matrix_coder::init_decode_invoker() {
            DECLARE_DECODE_INVOKER

            BIND_DECODE_INVOKER(ver_req);
            BIND_DECODE_INVOKER(ver_resp);

            BIND_DECODE_INVOKER(shake_hand_req);
            BIND_DECODE_INVOKER(shake_hand_resp);

            BIND_DECODE_INVOKER(get_peer_nodes_req);
            BIND_DECODE_INVOKER(get_peer_nodes_resp);

            BIND_DECODE_INVOKER(peer_nodes_broadcast_req);

            BIND_DECODE_INVOKER(node_create_task_req);
            BIND_DECODE_INVOKER(node_create_task_rsp);

            BIND_DECODE_INVOKER(node_start_task_req);
            BIND_DECODE_INVOKER(node_start_task_rsp);

            BIND_DECODE_INVOKER(node_stop_task_req);
            BIND_DECODE_INVOKER(node_stop_task_rsp);

            BIND_DECODE_INVOKER(node_restart_task_req);
            BIND_DECODE_INVOKER(node_restart_task_rsp);

            BIND_DECODE_INVOKER(node_reset_task_req);
            BIND_DECODE_INVOKER(node_reset_task_rsp);

            BIND_DECODE_INVOKER(node_destroy_task_req);
            BIND_DECODE_INVOKER(node_destroy_task_rsp);

            BIND_DECODE_INVOKER(node_list_task_req);
            BIND_DECODE_INVOKER(node_list_task_rsp);

            BIND_DECODE_INVOKER(node_task_logs_req);
            BIND_DECODE_INVOKER(node_task_logs_rsp);

            BIND_DECODE_INVOKER(show_req);
            BIND_DECODE_INVOKER(show_resp);

            BIND_DECODE_INVOKER(service_broadcast_req);
        }

        void matrix_coder::init_encode_invoker() {
            DECLARE_ENCODE_INVOKER

            BIND_ENCODE_INVOKER(ver_req);
            BIND_ENCODE_INVOKER(ver_resp);

            BIND_ENCODE_INVOKER(shake_hand_req);
            BIND_ENCODE_INVOKER(shake_hand_resp);

            BIND_ENCODE_INVOKER(get_peer_nodes_req);
            BIND_ENCODE_INVOKER(get_peer_nodes_resp);

            BIND_ENCODE_INVOKER(peer_nodes_broadcast_req);

            BIND_ENCODE_INVOKER(node_create_task_req);
            BIND_ENCODE_INVOKER(node_create_task_rsp);

            BIND_ENCODE_INVOKER(node_start_task_req);
            BIND_ENCODE_INVOKER(node_start_task_rsp);

            BIND_ENCODE_INVOKER(node_stop_task_req);
            BIND_ENCODE_INVOKER(node_stop_task_rsp);

            BIND_ENCODE_INVOKER(node_restart_task_req);
            BIND_ENCODE_INVOKER(node_restart_task_rsp);

            BIND_ENCODE_INVOKER(node_reset_task_req);
            BIND_ENCODE_INVOKER(node_reset_task_rsp);

            BIND_ENCODE_INVOKER(node_destroy_task_req);
            BIND_ENCODE_INVOKER(node_destroy_task_rsp);

            BIND_ENCODE_INVOKER(node_list_task_req);
            BIND_ENCODE_INVOKER(node_list_task_rsp);

            BIND_ENCODE_INVOKER(node_task_logs_req);
            BIND_ENCODE_INVOKER(node_task_logs_rsp);

            BIND_ENCODE_INVOKER(show_req);
            BIND_ENCODE_INVOKER(show_resp);

            BIND_ENCODE_INVOKER(service_broadcast_req);
        }

        std::shared_ptr<dbc::network::protocol> matrix_coder::get_protocol(int32_t type) {
            auto it = m_decode_protos.find(type);
            if (it == m_decode_protos.end()) {
                return nullptr;
            }

            return it->second;
        }

        /**
         * decode input message as an binary forward message if it match the fast forwarding conditions
         * @param ctx
         * @param in
         * @param msg
         * @param header
         * @param proto
         * @return DECODE_SUCCESS if no error happened
         */
        decode_status matrix_coder::decode_fast_forward(dbc::network::channel_handler_context &ctx,
                                                        byte_buf &in, std::shared_ptr<dbc::network::message> &msg,
                                                        dbc::network::base_header &header, std::shared_ptr<dbc::network::protocol> proto) {
            std::string local_node_id = get_local_node_id(ctx);
            if (local_node_id.empty()) {
                return DECODE_SUCCESS;
            }

            static std::vector<std::string> legacy_none_forwarding_messages{
                    "ver_req",
                    "ver_resp",
                    "shake_hand_req",
                    "shake_hand_resp",
                    "get_peer_nodes_req",
                    "get_peer_nodes_resp",
                    "service_broadcast_req"
            };

            if (find(legacy_none_forwarding_messages.begin(), legacy_none_forwarding_messages.end(),
                     header.msg_name)
                != legacy_none_forwarding_messages.end()) {
                // do not forward the legacy none forwarding message.
                return DECODE_SUCCESS;
            }


            if (header.exten_info.count("dest_id")
                && header.exten_info["dest_id"].find(local_node_id) == std::string::npos) {
                LOG_DEBUG << "forward " << header.msg_name;

                std::shared_ptr<dbc::network::msg_forward> content = std::make_shared<dbc::network::msg_forward>();

                content->header = header;

                byte_buf *buf = proto->get_buf();

                if (buf == nullptr) {
                    return DECODE_ERROR;
                }

                content->body.write_to_byte_buf(buf->get_read_ptr(), buf->get_valid_read_len());
                buf->move_read_ptr(buf->get_valid_read_len());

                msg->set_content(content);
//                        msg->set_name(content->header.msg_name);
                msg->set_name(BINARY_FORWARD_MSG);
            }

            return DECODE_SUCCESS;
        }


        decode_status
        matrix_coder::decode_frame(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &msg) {
            byte_buf uncompress_out;
            byte_buf &decode_in = in;

            try {
                // step 1: optional, uncompress packet
                if (dbc::network::matrix_compress::has_compress_flag(in)) {
                    if (!dbc::network::matrix_compress::uncompress(in, uncompress_out)) {
                        return DECODE_ERROR;
                    }

                    decode_in = uncompress_out;
                }

                // step 2: decode packet

                int32_t before_decode_len = decode_in.get_valid_read_len();

                //packet header
                matrix_packet_header packet_header;
                packet_header.read(decode_in);
                //get decode protocol
                std::shared_ptr<dbc::network::protocol> proto = get_protocol(packet_header.packet_type & 0xff);
                if (nullptr == proto) {
                    return DECODE_ERROR;
                } else {
                    proto->init_buf(&decode_in);
                }

                decode_status decodeRet = decode_service_frame(ctx, decode_in, msg, proto);
                if (E_SUCCESS == decodeRet) {
                    int32_t framelen = before_decode_len - decode_in.get_valid_read_len();
                    if (packet_header.packet_len != framelen) {
                        LOG_ERROR << "matrix msg_len error. msg_len in code frame is: " << packet_header.packet_len
                                  << "frame len is:" << framelen;
                        return DECODE_ERROR;
                    }
                }

                return decodeRet;
            }
            catch (std::exception &e) {
                LOG_ERROR << "matrix decode exception: " << e.what() << " " << decode_in.to_string();
                return DECODE_ERROR;
            }
            catch (...) {
                LOG_ERROR << "matrix decode exception: " << decode_in.to_string();
                return DECODE_ERROR;
            }

        }

        decode_status
        matrix_coder::decode_service_frame(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &msg,
                                           std::shared_ptr<dbc::network::protocol> proto) {
            //service header
            dbc::network::base_header header;
            header.read(proto.get());

            // jimmy: fast_forward processing
            decode_status rtn = decode_fast_forward(ctx, in, msg, header, proto);

            if (rtn != DECODE_SUCCESS) {
                return rtn;
            }

            if (msg->get_name() == std::string(BINARY_FORWARD_MSG)) {
                return rtn;
            }


            //find decoder
            auto it = m_binary_decode_invokers.find(header.msg_name);
            if (it == m_binary_decode_invokers.end()) {
                LOG_ERROR << "matrix decoder received unknown message: " << header.msg_name;
                //return DECODE_ERROR;
                return DECODE_UNKNOWN_MSG;
            }

            //body invoker
            auto invoker = it->second;
            invoker(msg, header, proto);

            return DECODE_SUCCESS;
        }

        template<typename msg_type>
        void matrix_coder::decode_invoke(std::shared_ptr<dbc::network::message> &msg, dbc::network::base_header &header,
                                         std::shared_ptr<dbc::network::protocol> &proto) {
            std::shared_ptr<msg_type> content = std::make_shared<msg_type>();

            content->header = header;
            content->body.read(proto.get());

            msg->set_content(content);
            msg->set_name(content->header.msg_name);

            //LOG_DEBUG << "matrix coder decode message: " << content->header.msg_name << ", message length: ";
        }

        encode_status matrix_coder::encode(dbc::network::channel_handler_context &ctx, dbc::network::message &msg, byte_buf &out) {

            try {
                matrix_packet_header packet_header;
                packet_header.write(out);

                // prepare protocol
                bool enable_compress = get_compress_enabled(ctx);
                uint32_t thrift_proto = get_thrift_proto(ctx);

                std::shared_ptr<dbc::network::protocol> proto = get_protocol(thrift_proto);

                if (proto == nullptr) {
                    LOG_ERROR << "matrix encoder unknown protocol value: " << thrift_proto;
                    return ENCODE_ERROR;
                }

                proto->init_buf(&out);

                // encode
                std::shared_ptr<dbc::network::msg_forward> bin_fwd_content = std::dynamic_pointer_cast<dbc::network::msg_forward>(
                        msg.content);
                if (bin_fwd_content) {
                    LOG_DEBUG << "matrix encoder forward binary of " << msg.get_name();

                    bin_fwd_content->header.write(proto.get());
                    proto->append_raw(bin_fwd_content->body.get_read_ptr(), bin_fwd_content->body.get_valid_read_len());

                } else {
                    //find encoder
                    auto it = m_binary_encode_invokers.find(msg.get_name());
                    if (it == m_binary_encode_invokers.end()) {
                        LOG_ERROR << "matrix encoder received unknown message: " << msg.get_name();
                        return ENCODE_ERROR;
                    }

                    //body invoker
                    auto invoker = it->second;
                    invoker(ctx, proto, msg, out);
                }

                //update packet len and packet type
                packet_header.packet_len = out.get_valid_read_len();
                packet_header.packet_type = thrift_proto;
                if (!packet_header.update(out)) {
                    return ENCODE_ERROR;
                }

                // compress
                if (enable_compress && packet_header.packet_len >= dbc::network::matrix_compress::MIN_MSG_LEN_TO_COMPRESS) {
                    if (!dbc::network::matrix_compress::compress(out)) {
                        return ENCODE_ERROR;
                    }
                }

            }
            catch (std::exception &e) {
                LOG_ERROR << "matrix encode message error: " << e.what();
                return ENCODE_ERROR;
            }
            catch (...) {
                LOG_ERROR << "matrix encode message error!";
                return ENCODE_ERROR;
            }

            return ENCODE_SUCCESS;
        }

        template<typename msg_type>
        void matrix_coder::encode_invoke(dbc::network::channel_handler_context &ctx, std::shared_ptr<dbc::network::protocol> &proto, dbc::network::message &msg,
                                         byte_buf &out) {
            std::shared_ptr<msg_type> content = std::dynamic_pointer_cast<msg_type>(msg.content);

            content->header.write(proto.get());
            content->body.write(proto.get());
        }

        decode_status matrix_coder::recv_message(byte_buf &in) {
            while (in.get_valid_read_len() > 0) {
                if (m_recv_messages.empty() || m_recv_messages.back().complete()) {
                    m_recv_messages.push_back(net_message(DEFAULT_BUF_LEN));
                }

                net_message &msg = m_recv_messages.back();

                int32_t handled = 0;
                if (!msg.has_data()) {
                    handled = msg.read_packet_len(in);
                } else {
                    handled = msg.read_packet_body(in);
                }

                if (handled < 0) {
                    return DECODE_ERROR;
                }

                if (msg.has_data() &&
                    (msg.get_packetlen() > MAX_MATRIX_MSG_LEN || msg.get_packetlen() < MIN_MATRIX_MSG_CODE_LEN)) {
                    LOG_ERROR << "Oversized message";
                    return DECODE_ERROR;
                }
            }
            return DECODE_SUCCESS;
        }


        bool matrix_coder::get_compress_enabled(dbc::network::channel_handler_context &ctx) {
            bool rtn = false;

            variables_map &vm = ctx.get_args();
            try {
                if (vm.count("ENABLE_COMPRESS")) {
                    if (vm["ENABLE_COMPRESS"].as<bool>()) {
                        rtn = true;
                    }
                }

            }
            catch (...) {

            }

            return rtn;
        }

        int matrix_coder::get_thrift_proto(dbc::network::channel_handler_context &ctx) {
            int rtn = THRIFT_BINARY_PROTO;

            variables_map &vm = ctx.get_args();
            try {
                if (vm.count("THRIFT_PROTO")) {
                    rtn = vm["THRIFT_PROTO"].as<int>();
                }
            }
            catch (...) {

            }

            return rtn;
        }

        std::string matrix_coder::get_local_node_id(dbc::network::channel_handler_context &ctx) {
            std::string rtn;

            variables_map &vm = ctx.get_args();
            try {
                if (vm.count("LOCAL_NODE_ID")) {
                    rtn = vm["LOCAL_NODE_ID"].as<string>();
                }
            }
            catch (...) {

            }

            return rtn;
        }


        matrix_packet_header::matrix_packet_header()
                : packet_len(0), packet_type(0) {

        }

        void matrix_packet_header::write(byte_buf &out) {
            int32_t len = dbc::network::byte_order::hton32(packet_len);
            out.write_to_byte_buf((char *) &len, sizeof(len));

            int32_t type = dbc::network::byte_order::hton32(packet_type);
            out.write_to_byte_buf((char *) &type, sizeof(type));
        }

        bool matrix_packet_header::update(byte_buf &out) {
            if (packet_len != out.get_valid_read_len() && packet_len != out.get_valid_write_len()) {
                LOG_ERROR << "packet_len != byte_buf size";
                return false;
            }

            int32_t len = dbc::network::byte_order::hton32(packet_len);
            memcpy(out.get_read_ptr(), &len, sizeof(len));

            int32_t type = dbc::network::byte_order::hton32(packet_type);
            memcpy(out.get_read_ptr() + sizeof(type), &type, sizeof(type));

            return true;
        }

        void matrix_packet_header::read(byte_buf &in) {
            auto size = sizeof(packet_len) + sizeof(packet_type);
            assert(in.get_valid_read_len() >= size);

            //packet len + packet type
            const int32_t *ptr = (int32_t *) in.get_read_ptr();
            packet_len = dbc::network::byte_order::ntoh32(*ptr);
            packet_type = dbc::network::byte_order::ntoh32(*(ptr + 1));

            in.move_read_ptr(size);                    //be careful of size
        }

    }

}

