// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "codec/matrix_coder.h"
#include "service_message_id.h"
#include <iostream>
#include <memory>

using namespace matrix::service_core;

BOOST_AUTO_TEST_SUITE(suite_codec, * utf::enabled())

    BOOST_AUTO_TEST_CASE(test_encode){

        message msg;
        channel_handler_context ctx;
        byte_buf buf;

        matrix_coder coder;

        msg.header.msg_name = P2P_GET_PEER_NODES_RESP;
        msg.header.msg_priority = 0;

        auto resp_content = std::make_shared<get_peer_nodes_resp>();
        resp_content->header.magic = TEST_NET;
        resp_content->header.msg_name = P2P_GET_PEER_NODES_RESP;
        resp_content->header.__set_nonce("12345");

        matrix::service_core::peer_node_info info;
        info.peer_node_id = "node1";
        info.live_time_stamp = 1;
        info.addr.ip = "192.168.0.2";
        info.addr.port = 10001;
        info.service_list.push_back(std::string("ai_training"));
        resp_content->body.peer_nodes_list.push_back(std::move(info));

        msg.set_content(std::dynamic_pointer_cast<matrix::core::base>(resp_content));

        auto r = coder.encode(ctx, msg, buf);

//        std::cout<<"buf: "<< buf.to_string()<<std::endl;
        BOOST_TEST(r==ENCODE_SUCCESS);

    }

    BOOST_AUTO_TEST_CASE(test_encode_wo_required_field){

        message msg;
        channel_handler_context ctx;
        byte_buf buf;

        matrix_coder coder;

        msg.header.msg_name = P2P_GET_PEER_NODES_RESP;
        msg.header.msg_priority = 0;

        auto resp_content = std::make_shared<get_peer_nodes_resp>();
//        resp_content->header.magic = TEST_NET;
//        resp_content->header.msg_name = P2P_GET_PEER_NODES_RESP;
        resp_content->header.__set_nonce("12345");

        matrix::service_core::peer_node_info info;
        info.peer_node_id = "node1";
        info.live_time_stamp = 1;
        info.addr.ip = "192.168.0.2";
        info.addr.port = 10001;
        info.service_list.push_back(std::string("ai_training"));
        resp_content->body.peer_nodes_list.push_back(std::move(info));

        msg.set_content(std::dynamic_pointer_cast<matrix::core::base>(resp_content));

        auto r = coder.encode(ctx, msg, buf);

        std::cout<<"buf: "<< buf.to_string()<<std::endl;
        BOOST_TEST(r==ENCODE_SUCCESS);

    }


    #include <cstdint>


// tips: generate bytes for c++ from message dump
//    python
//    s="00 00 00 80 00 00 00 00 7F 7F".split(" ")
//    s=["0x"+e for e in s]
//    ",".join(s)
    BOOST_AUTO_TEST_CASE(test_decode){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                  0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                  0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70, 0x7F, 0x7F};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(msg->header.msg_name == "shake_hand_resp");
        BOOST_TEST(r==DECODE_SUCCESS);

    }

    BOOST_AUTO_TEST_CASE(test_decode_missing_T_STOP){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                  0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                  0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r==DECODE_ERROR);
    }

    BOOST_AUTO_TEST_CASE(test_decode_empty_required_field){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x08,0x00,0x01,0x00,0x00,0x00,0x00,
                           0x0B,0x00,0x02,0x00,0x00,0x00,0x00,
                           0x0B,0x00,0x03,0x00,0x00,0x00,0x05,0x31,0x32,0x33,0x34,0x35,0x7F,
                           0x0F,0x00,0x01,0x0C,0x00,0x00,0x00,0x01,0x0B,0x00,0x01,0x00,0x00,0x00,0x05,0x6E,0x6F,0x64,0x65,0x31,0x08,0x00,0x02,0x00,0x00,0x00,0x00,0x08,0x00,0x03,0x00,0x00,0x00,0x00,0x08,0x00,0x04,0x00,0x00,0x00,0x01,0x0C,0x00,0x05,0x0B,0x00,0x01,0x00,0x00,0x00,0x0B,0x31,0x39,0x32,0x2E,0x31,0x36,0x38,0x2E,0x30,0x2E,0x32,0x06,0x00,0x02,0x27,0x11,0x7F,0x0F,0x00,0x06,0x0B,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0B,0x61,0x69,0x5F,0x74,0x72,0x61,0x69,0x6E,0x69,0x6E,0x67,0x7F,0x7F};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(msg->header.msg_name == "unknown message");
        BOOST_TEST(r==DECODE_ERROR);
    }

    BOOST_AUTO_TEST_CASE(test_decode_abnormal_packet_lenght){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                           0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                           0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r==DECODE_LENGTH_IS_NOT_ENOUGH);
    }

    BOOST_AUTO_TEST_CASE(test_decode_extremely_long_packet){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                           0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                           0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70, 0x7F, 0x7F};

        uint8_t c[1024000]={0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97};

        for(int i=15; i< sizeof(c); i++){
            c[i] =0x01;
        }


        buf.write_to_byte_buf((const char*)c, sizeof(c));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);


        BOOST_TEST(r!=DECODE_SUCCESS);
    }

    BOOST_AUTO_TEST_CASE(test_decode_extremely_short_packet){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                           0x0B};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r!=DECODE_SUCCESS);
    }

    BOOST_AUTO_TEST_CASE(test_decode_abnormal_string_length){
        channel_handler_context ctx;
        byte_buf buf;

        int i = 4;
        string text = "Player ";
        std::cout << "player" + std::to_string(i) <<std::endl;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                           0x0B, 0x00, 0x02, 0x0F, 0xFF, 0xFF, 0xFF, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                           0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70, 0x7F, 0x7F};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r!=DECODE_SUCCESS);
    }

//    BOOST_AUTO_TEST_CASE(test_decode_list_recursive){
//        channel_handler_context ctx;
//        byte_buf buf;
//
//        const uint8_t b[]={0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
//                           0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
//                           0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70, 0x7F,
//                           0x0F, 0x00, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x01,0x7F,
//                           0x7F};
//
//        buf.write_to_byte_buf((const char*)b, sizeof(b));
//
//        matrix_coder coder;
//
//        shared_ptr<message> msg = std::make_shared<message>();
//
//        decode_status r = coder.decode(ctx, buf, msg);
//
//        BOOST_TEST(r!=ENCODE_SUCCESS);
//    }

    BOOST_AUTO_TEST_CASE(test_decode_wo_msg_name_field){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                           0x7F, 0x7F};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r!=DECODE_SUCCESS);
    }

    BOOST_AUTO_TEST_CASE(test_decode_wo_magic_field){
        channel_handler_context ctx;
        byte_buf buf;

        const uint8_t b[]={0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x00,
                           0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                           0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70,
                           0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
                           0x7F, 0x7F};

        buf.write_to_byte_buf((const char*)b, sizeof(b));

        matrix_coder coder;

        shared_ptr<message> msg = std::make_shared<message>();

        decode_status r = coder.decode(ctx, buf, msg);

        BOOST_TEST(r!=DECODE_SUCCESS);
    }


BOOST_AUTO_TEST_SUITE_END()