/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         :   compress.cpp
* description       :
* date              :   2018.09.27
* author            :
**********************************************************************************/


#include "matrix_compress.h"
#include "snappy/snappy.h"
#include "codec/protocol.h"
#include "log/log.h"

namespace matrix{
    using namespace core;

    namespace core {

    /**
     * compress data stored in byte buf; replace input data with compressed data.
     * @param inout, buffer of input data at invoking;  reset the buffer content with the compressed data
     * @param proto_type
     */
    void matrix_compress::compress(byte_buf& inout, bool force)
    {
        //compress: message header and body
        //do not compress packet header, but update the length filed in packet header: length = 8 + new compressed data.

        uint32_t in_msg_len = inout.get_valid_read_len();

        if (!force && in_msg_len < MIN_MSG_LEN_TO_COMPRESS)
        {
            LOG_DEBUG << "msg len less than 256, skip compress";
            return;
        }

        char *p = inout.get_read_ptr();


        // protocol type: 4 bytes (network order)
        //
        //      the 4th byte, thrift protocol: binary, compact ...
        //      the 3rd byte, compress: no (0), yes (1)
        uint32_t proto_type = *((uint32_t*)(p + PACKET_LEN_FIELD_OFFSET)); //keep the network order
        char* q = (char*) &proto_type;
        *(q+2) = COMPRESS;


        std::string ctext;

        bool enable_skip_bytes = false;
        snappy::Compress(p + PACKET_HEADER_LEN, in_msg_len - PACKET_HEADER_LEN, &ctext, enable_skip_bytes);

        // fill the compressed data into byte_buf
        inout.reset();

        uint32_t out_msg_len = PACKET_HEADER_LEN + ctext.length();
        uint32_t out_msg_len_net = byte_order::hton32(out_msg_len);

        inout.write_to_byte_buf((char *) &out_msg_len_net, sizeof(out_msg_len_net));
        inout.write_to_byte_buf((char *) &proto_type, sizeof(proto_type));
        inout.write_to_byte_buf(ctext.c_str(), ctext.length());

        LOG_DEBUG << "compress size: " << in_msg_len << " --> " << out_msg_len;
    }

    /**
     *
     * @param in
     * @return
     */
    bool matrix_compress::has_compress_flag(byte_buf& in)
    {
        char *p = in.get_read_ptr();

        uint32_t proto_type = *((uint32_t*)(p + PACKET_LEN_FIELD_OFFSET));
        proto_type = byte_order::ntoh32(proto_type);

        return (proto_type>>8) & 0x01;

    }

    /**
     *
     * @param in,   input compressed data, preceed a packet header
     * @param out,  uncompressed data
     * @return true if uncompress complete, otherwise return false
     */
    bool matrix_compress::uncompress(byte_buf& in, byte_buf& out)
    {
        LOG_DEBUG << "uncompress";

        if(!has_compress_flag(in))
        {
            LOG_ERROR << "try to uncompress a packet without compress flag";
            return false;
        }

        std::string text;
        char *p = in.get_read_ptr();
        size_t len = in.get_valid_read_len();

        //check if the packet is valid
        uint32_t packet_len = byte_order::ntoh32(*((uint32_t*)p));
        if (len == packet_len)
        {
            LOG_ERROR << "input buf size "<< len << " != packet.len " << packet_len;
            return false;
        }

        //step 1: invoke snappy uncompress
        if (!snappy::Uncompress(p + PACKET_HEADER_LEN, len - PACKET_HEADER_LEN, &text))
        {
            LOG_ERROR << "uncompress failed";
            return false;
        }

        //step 2: set packet len
        uint32_t msg_len = PACKET_HEADER_LEN + text.length();
        msg_len = byte_order::hton32(msg_len);
        out.write_to_byte_buf((char *) &msg_len, sizeof(msg_len));

        //step 3: set proto type: remove compress flag
        uint32_t proto_type = *((uint32_t*)(p + PACKET_LEN_FIELD_OFFSET));
        proto_type = byte_order::ntoh32(proto_type);
        proto_type &= 0xFFFF00FF;
        proto_type = byte_order::hton32(proto_type);

        out.write_to_byte_buf((char*)&proto_type, PACKET_PROTO_TYPE_FIELD_SIZE);

        //step 4: cp uncompressed text into output buf
        out.write_to_byte_buf(text.c_str(), text.length());

        return true;
    }

    }
}
