#ifndef DBC_LENGTH_FIELD_FRAME_DECODER_H
#define DBC_LENGTH_FIELD_FRAME_DECODER_H

#include <stdexcept>
#include "util/memory/byte_buf.h"
#include "network/protocol/service_message.h"
#include "decoder.h"
#include "net_message.h"
#include "log/log.h"

#define MIN_MATRIX_MSG_CODE_LEN                         30                                //MIN MSG_LEN in code frame
//#define MAX_MATRIX_MSG_CODE_LEN                            10240                              //MAX MSG_LEN in code frame
//using namespace matrix::service_core;

class length_field_frame_decoder : public decoder
{
public:

    length_field_frame_decoder(uint32_t min_read_length, uint32_t max_frame_len)
        : m_min_read_length(min_read_length)
        , m_max_frame_len(max_frame_len)
    {
    }

    bool has_complete_message()
    {
        if (m_recv_messages.size() > 0)
        {
            return m_recv_messages.front().complete();
        }

        return false;
    };

    //virtual decode_status decode(channel_handler_context &ctx, byte_buf &in, std::shared_ptr<message> &message)
    //{
    //    //less than complete length field len
    //    if (in.get_valid_read_len() < m_min_read_length)
    //    {
    //        return DECODE_ERROR;
    //    }

    //    //msg_len offset is 6 bytes and size is 4 bytes
    //    uint64_t frame_len = get_unadjusted_frame_length(in, 0, 4);
    //    if (frame_len > m_max_frame_len)
    //    {
    //        LOG_ERROR << "matrix decode msg_len too long: " << frame_len;
    //        return DECODE_ERROR;
    //    }

    //    //less than complete msg frame len
    //    if (in.get_valid_read_len() < frame_len)
    //    {
    //        return DECODE_NEED_MORE_DATA;
    //    }

    //    if (frame_len < MIN_MATRIX_MSG_CODE_LEN)
    //    {
    //        LOG_ERROR << "matrix decode msg_len too short: " << frame_len;
    //        return DECODE_ERROR;
    //    }

    //    //decode frame
    //    decode_status status = decode_frame(ctx, in, message);
    //    if (status != DECODERR_SUCCESS)
    //    {
    //        return status;
    //    }

    //    m_recv_messages.pop_front();
    //    //decode frame
    //    return DECODERR_SUCCESS;
    //}

    virtual decode_status decode(dbc::network::channel_handler_context &ctx, std::shared_ptr<dbc::network::message> &message)
    {
        net_message &net_msg = m_recv_messages.front();
        LOG_DEBUG << "decode buf: " << net_msg.get_message_stream().to_string();
        if (!net_msg.complete())
        {
            m_recv_messages.push_back(std::move(net_msg));
            m_recv_messages.pop_front();
            return DECODE_NEED_MORE_DATA;
        }

//                LOG_DEBUG << "decode net message buf:" << net_msg.get_message_stream().to_string();
        //less than complete length field len
        if (net_msg.get_message_stream().get_valid_read_len() < m_min_read_length)
        {
            return DECODE_ERROR;
        }

        //msg_len offset is 4 bytes and size is 4 bytes
        uint64_t frame_len = get_unadjusted_frame_length(net_msg.get_message_stream(), 0, 4);
        if (frame_len > m_max_frame_len)
        {
            LOG_ERROR << "matrix decode msg_len too long: " << frame_len;
            return DECODE_ERROR;
        }

        //less than complete msg frame len
        if (net_msg.get_message_stream().get_valid_read_len() < frame_len)
        {
            return DECODE_NEED_MORE_DATA;
        }

        if (frame_len < MIN_MATRIX_MSG_CODE_LEN)
        {
            LOG_ERROR << "matrix decode msg_len too short: " << frame_len;
            return DECODE_ERROR;
        }

        //decode frame
        decode_status status = decode_frame(ctx, net_msg.get_message_stream(), message);

        m_recv_messages.pop_front();
        if (status != DECODERR_SUCCESS)
        {
            return status;
        }

        return DECODERR_SUCCESS;
    }

    virtual decode_status recv_message(byte_buf &in) = 0;
protected:

    virtual decode_status decode_frame(dbc::network::channel_handler_context &ctx, byte_buf &in, std::shared_ptr<dbc::network::message> &message) = 0;

    uint64_t get_unadjusted_frame_length(byte_buf &in, uint32_t offset, uint32_t size)
    {
        switch (size)
        {
        case 1:
        {
            uint8_t frame_len;

            char b[1];
            memcpy(b, in.get_read_ptr() + offset, 1);
            frame_len = *(uint8_t*)b;

            return frame_len;
        }
        case 2:
        {
            uint16_t frame_len;

            union bytes
            {
                char b[2];
                int16_t all;
            } theBytes;
            memcpy(theBytes.b, in.get_read_ptr() + offset, 2);
            frame_len = (uint16_t) dbc::network::byte_order::ntoh16(theBytes.all);

            return frame_len;
        }
        case 4:
        {
            uint32_t frame_len;

            union bytes
            {
                char b[4];
                int32_t all;
            } theBytes;
            memcpy(theBytes.b, in.get_read_ptr() + offset, 4);
            frame_len = (uint32_t) dbc::network::byte_order::ntoh32(theBytes.all);

            return frame_len;
        }
        case 8:
        {
            uint64_t frame_len;

            union bytes
            {
                char b[8];
                int64_t all;
            } theBytes;
            memcpy(theBytes.b, in.get_read_ptr() + offset, 8);
            frame_len = (uint64_t) dbc::network::byte_order::ntoh64(theBytes.all);

            return frame_len;
        }
        default:
            throw std::invalid_argument("length_field_frame_decoder invalid size");
        }
    }

protected:

    uint32_t m_min_read_length;

    uint32_t m_max_frame_len;

    std::list<net_message> m_recv_messages;

};

#endif
