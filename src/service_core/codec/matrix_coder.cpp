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
            : length_field_frame_decoder(MATRIX_MSG_LENGTH_FIELD_END_OFFSET, MAX_MATRIX_MSG_LEN)
        {
            init_decode_invoker();
        }

        void matrix_coder::init_decode_invoker()
        {
            decode_invoker_type invoker;
            
            invoker = std::bind(&matrix_coder::decode_invoker<ver_req>, this, std::placeholders::_1, std::placeholders::_2);
            m_decode_invokers.insert({ VER_REQ,{ invoker } });

            invoker = std::bind(&matrix_coder::decode_invoker<ver_resp>, this, std::placeholders::_1, std::placeholders::_2);
            m_decode_invokers.insert({ VER_RESP,{ invoker } });

            invoker = std::bind(&matrix_coder::decode_invoker<shake_hand_req>, this, std::placeholders::_1, std::placeholders::_2);
            m_decode_invokers.insert({ SHAKE_HAND_REQ,{ invoker } });

            invoker = std::bind(&matrix_coder::decode_invoker<shake_hand_resp>, this, std::placeholders::_1, std::placeholders::_2);
            m_decode_invokers.insert({ SHAKE_HAND_RESP,{ invoker } });
        }

        decode_status matrix_coder::decode_frame(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &msg)
        {
            uint32_t magic = 0;
            uint32_t msg_len = 0;
            std::string msg_name;

            //try decode header key fields
            auto status = decode_header_fields(ctx, in, msg_len, magic, msg_name);
            if (DECODE_SUCCESS != status)
            {
                return status;
            }

            try
            {
                auto it = m_decode_invokers.find(msg_name);
                if (it == m_decode_invokers.end())
                {
                    LOG_ERROR << "matrix decoder received unknown message: " << msg_name;
                    return DECODE_ERROR;
                }

                auto decode_invoker = it->second;
                std::shared_ptr<binary_protocol> proto(new binary_protocol(&in));
                decode_invoker(msg, proto);
            }
            catch (std::exception &e)
            {
                LOG_ERROR << "matrix decode exception: " << e.what();
                return DECODE_ERROR;
            }
            catch (...)
            {
                LOG_ERROR << "matrix decode exception!";
                return DECODE_ERROR;
            }

            return DECODE_SUCCESS;

        }

        template<typename msg_type>
        void matrix_coder::decode_invoker(std::shared_ptr<message> &msg, std::shared_ptr<binary_protocol> &proto)
        {
            uint32_t xfer = 0;
            std::shared_ptr<msg_type> content(new msg_type);
            xfer = content->read(proto.get());
            msg->set_content(content);
            msg->set_name(content->header.msg_name);
        }

        decode_status matrix_coder::decode_header_fields(channel_handler_context &ctx, byte_buf &in, uint32_t &msg_len, uint32_t &magic, std::string &msg_name)
        {
            //key header fields
            uint8_t ftype = 0;
            uint16_t fid = 0;

            char *ptr = in.get_read_ptr();

            //skip struct begin
            ptr += 3;

            //msg_len
            ftype = *ptr++;
            fid = *((uint16_t *)ptr);
            fid = byte_order::ntoh16(fid);
            ptr += 2;
            if (1 != fid || T_I32 != ftype)
            {
                return DECODE_ERROR;
            }

            memcpy(&msg_len, ptr, sizeof(msg_len));
            msg_len = byte_order::ntoh32(msg_len);
            ptr += sizeof(msg_len);

            //magic
            ftype = *ptr++;
            fid = *((uint16_t *)ptr);
            fid = byte_order::ntoh16(fid);
            ptr += 2;
            if (2 != fid || T_I32 != ftype)
            {
                return DECODE_ERROR;
            }

            memcpy(&magic, ptr, sizeof(magic));
            magic = byte_order::ntoh32(magic);
            ptr += sizeof(magic);

            //msg_name string length
            ftype = *ptr++;
            fid = *((uint16_t *)ptr);
            fid = byte_order::ntoh16(fid);
            ptr += 2;
            if (3 != fid || T_STRING != ftype)
            {
                return DECODE_ERROR;
            }

            uint32_t msg_name_len = 0;
            memcpy(&msg_name_len, ptr, sizeof(msg_name_len));
            msg_name_len = byte_order::ntoh32(msg_name_len);
            ptr += sizeof(msg_name_len);

            //msg_name string
            if (msg_name_len > (in.get_valid_read_len() - DEFAULT_DECODE_HEADER_LEN))
            {
                return DECODE_ERROR;
            }

            msg_name.resize(msg_name_len, 0x00);
            memcpy(&msg_name[0], ptr, msg_name_len);

            return DECODE_SUCCESS;
        }

    }

}

