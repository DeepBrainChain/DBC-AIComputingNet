/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºdbc_decoder.cpp
* description    £ºdbc decoder for network transport
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "matrix_coder.h"
#include "service_message_id.h"


namespace matrix
{
    namespace service_core
    {

        matrix_coder::matrix_coder() 
            : length_field_frame_decoder(MATRIX_MSG_MIN_READ_LENGTH, MAX_MATRIX_MSG_LEN)
        {
            init_decode_proto();
            
            init_decode_invoker();
            init_encode_invoker();
        }

        void matrix_coder::init_decode_proto()
        {
            m_decode_protos[THRIFT_BINARY_PROTO] = make_shared<binary_protocol>();
        }

        void matrix_coder::init_decode_invoker()
        {
            DECLARE_DECODE_INVOKER
            
            BIND_DECODE_INVOKER(ver_req);
            BIND_DECODE_INVOKER(ver_resp);

            BIND_DECODE_INVOKER(shake_hand_req);
            BIND_DECODE_INVOKER(shake_hand_resp);

            BIND_DECODE_INVOKER(get_peer_nodes_req);
            BIND_DECODE_INVOKER(get_peer_nodes_resp);

            BIND_DECODE_INVOKER(peer_nodes_broadcast_req);

            BIND_DECODE_INVOKER(start_training_req);
            BIND_DECODE_INVOKER(stop_training_req);

            BIND_DECODE_INVOKER(list_training_req);
            BIND_DECODE_INVOKER(list_training_resp);

            BIND_DECODE_INVOKER(logs_req);
            BIND_DECODE_INVOKER(logs_resp);
        }

        void matrix_coder::init_encode_invoker()
        {
            DECLARE_ENCODE_INVOKER

            BIND_ENCODE_INVOKER(ver_req);
            BIND_ENCODE_INVOKER(ver_resp);

            BIND_ENCODE_INVOKER(shake_hand_req);
            BIND_ENCODE_INVOKER(shake_hand_resp);

            BIND_ENCODE_INVOKER(get_peer_nodes_req);
            BIND_ENCODE_INVOKER(get_peer_nodes_resp);

            BIND_ENCODE_INVOKER(peer_nodes_broadcast_req);

            BIND_ENCODE_INVOKER(start_training_req);
            BIND_ENCODE_INVOKER(stop_training_req);

            BIND_ENCODE_INVOKER(list_training_req);
            BIND_ENCODE_INVOKER(list_training_resp);

            BIND_ENCODE_INVOKER(logs_req);
            BIND_ENCODE_INVOKER(logs_resp);

        }

        std::shared_ptr<protocol> matrix_coder::get_protocol(int32_t type)
        {
            auto it = m_decode_protos.find(type);
            if (it == m_decode_protos.end())
            {
                return nullptr;
            }

            return it->second;
        }

        decode_status matrix_coder::decode_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg)
        {
            try
            {
                uint32_t before_decode_len = in.get_valid_read_len();

                //packet header
                matrix_packet_header packet_header;
                decode_packet_header(in, packet_header);
                //get decode protocol
                std::shared_ptr<protocol> proto = get_protocol(packet_header.packet_type);
                if (nullptr == proto)
                {
                    return DECODE_ERROR;
                }
                else
                {
                    proto->init_buf(&in);
                }
                decode_status decodeRet = decode_service_frame(ctx, in, msg, proto);
                if (E_SUCCESS == decodeRet)
                {
                    uint32_t framelen = before_decode_len - in.get_valid_read_len();
                    if (packet_header.packet_len != framelen)
                    {
                        LOG_ERROR << "matrix msg_len error. msg_len in code frame is: " << packet_header.packet_len << "frame len is:" << framelen;
                        return DECODE_ERROR;
                    }
                }
          
                return decodeRet;
            }
            catch (std::exception &e)
            {
                LOG_ERROR << "matrix decode exception: " << e.what() << " " << in.to_string();
                return DECODE_ERROR;
            }
            catch (...)
            {
                LOG_ERROR << "matrix decode exception: " << in.to_string();
                return DECODE_ERROR;
            }

        }

        void matrix_coder::decode_packet_header(byte_buf &in, matrix_packet_header &packet_header)
        {
            assert(in.get_valid_read_len() >= sizeof(matrix_packet_header));

            //packet len + packet type
            int32_t *ptr = (int32_t *)in.get_read_ptr();
            packet_header.packet_len = byte_order::ntoh32(*ptr);
            packet_header.packet_type = byte_order::ntoh32(*(ptr + 1));

            in.move_read_ptr(sizeof(matrix_packet_header));                    //be careful of size
        }

        void matrix_coder::encode_packet_header(matrix_packet_header &packet_header, byte_buf &out)
        {
            //net endian
            packet_header.packet_len = 0;
            packet_header.packet_type = THRIFT_BINARY_PROTO;

            //packet len + packet type
            out.write_to_byte_buf((char*)&packet_header.packet_len, 4);
            out.write_to_byte_buf((char*)&packet_header.packet_type, 4);
        }

        decode_status matrix_coder::decode_service_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg, std::shared_ptr<protocol> proto)
        {
            //service header
            msg_header header;
            header.read(proto.get());

            //find decoder
            auto it = m_binary_decode_invokers.find(header.msg_name);
            if (it == m_binary_decode_invokers.end())
            {
                LOG_ERROR << "matrix decoder received unknown message: " << header.msg_name;
                return DECODE_ERROR;
            }

            //body invoker
            auto invoker = it->second;
            invoker(msg, header, proto);

            //pass context to caller
            variable_value val;
            val.value() = header.__isset.nonce ? header.nonce : DEFAULT_STRING;
            ctx.get_args().insert(std::make_pair("nonce", val));
            
            return DECODE_SUCCESS;
        }

        template<typename msg_type>
        void matrix_coder::decode_invoke(std::shared_ptr<message> &msg, msg_header &header, std::shared_ptr<protocol> &proto)
        {
            std::shared_ptr<msg_type> content(new msg_type);

            content->header = header;
            content->body.read(proto.get());

            msg->set_content(content);
            msg->set_name(content->header.msg_name);

            //LOG_DEBUG << "matrix coder decode message: " << content->header.msg_name << ", message length: ";
        }

        encode_status matrix_coder::encode(channel_handler_context &ctx, message & msg, byte_buf &out)
        {

            try
            {
                matrix_packet_header packet_header;
                encode_packet_header(packet_header, out);

                //find encoder
                auto it = m_binary_encode_invokers.find(msg.get_name());
                if (it == m_binary_encode_invokers.end())
                {
                    LOG_ERROR << "matrix encoder received unknown message: " << msg.get_name();
                    return ENCODE_ERROR;
                }

                std::shared_ptr<protocol> proto = get_protocol(THRIFT_BINARY_PROTO);
                assert(proto != nullptr);
                proto->init_buf(&out);

                //body invoker
                auto invoker = it->second;
                invoker(ctx, proto, msg, out);

                //get msg length and net endiuan and fill in
                uint32_t msg_len = out.get_valid_read_len();
                msg_len = byte_order::hton32(msg_len);
                memcpy(out.get_read_ptr(), &msg_len, sizeof(msg_len));
            }
            catch (std::exception &e)
            {
                LOG_ERROR << "matrix encode message error: " << e.what();
                return ENCODE_ERROR;
            }
            catch (...)
            {
                LOG_ERROR << "matrix encode message error!";
                return ENCODE_ERROR;
            }

            return ENCODE_SUCCESS;
        }

        template<typename msg_type>
        void matrix_coder::encode_invoke(channel_handler_context &ctx, std::shared_ptr<protocol> &proto, message & msg, byte_buf &out)
        {
            std::shared_ptr<msg_type> content = std::dynamic_pointer_cast<msg_type>(msg.content);            

            content->header.write(proto.get());
            content->body.write(proto.get());

            //pass context to caller
            variable_value val;
            val.value() = content->header.__isset.nonce ? content->header.nonce : DEFAULT_STRING;
            ctx.get_args().insert(std::make_pair("nonce", val));
        }

    }

}

