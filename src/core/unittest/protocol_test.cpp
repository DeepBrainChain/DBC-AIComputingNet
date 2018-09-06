/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : protocol_test.cpp
* description       : test protocol.cpp
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <sstream>
#include "common_def.h"
#include "protocol.h"

using namespace matrix::core;

BOOST_AUTO_TEST_CASE(test_protocol_read)
{
  // 不包括：消息包长度/ 消息包类型
  const uint8_t b[]={0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                  0x0B, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                  0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70, 0x7F, 0x7F};
  
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
  buf->write_to_byte_buf((const char*)b, sizeof(b));

  binary_protocol proto(buf.get());

  std::shared_ptr<base_header> header = std::make_shared<base_header>();
  header->read(&proto);

  BOOST_TEST(header->msg_name == "shake_hand_resp");
}

// list element is uint32_t
BOOST_AUTO_TEST_CASE(test_protocol_read_list)
{
  // 不包括：消息包长度/ 消息包类型
  const uint8_t b[]= {0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                    0xf, 0x00, 0x05, 0x08, 0x00, 0x00, 0x00, 0x04,
                    0x00, 0x00, 0x00, 0x04, 0x7f};
 
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
  buf->write_to_byte_buf((const char*)b, sizeof(b));

  binary_protocol proto(buf.get());

  std::shared_ptr<base_header> header = std::make_shared<base_header>();

  BOOST_CHECK_THROW(header->read(&proto), std::length_error);
}

// map element is uint32_t
BOOST_AUTO_TEST_CASE(test_protocol_read_map)
{
  // 不包括：消息包长度/ 消息包类型
  const uint8_t b[]= {0x08, 0x00, 0x01, 0xE1, 0xD1, 0xA0, 0x97,
                    0xd, 0x00, 0xff, 0x08, 0x00, 0x00, 0x00, 0x04,
                    0x08, 0x08, 0x00, 0x04, 0x7f};
 
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
  buf->write_to_byte_buf((const char*)b, sizeof(b));

  binary_protocol proto(buf.get());

  std::shared_ptr<base_header> header = std::make_shared<base_header>();

  BOOST_CHECK_THROW(header->read(&proto), std::length_error);
}

BOOST_AUTO_TEST_CASE(test_protocol_write)
{
  // magic
  std::shared_ptr<base_header> header = std::make_shared<base_header>();
  header->__set_magic(0xE1D1A097);

  // msg_name:shake_hand_resp
  uint8_t msg_name[] = {0x73, 0x68, 0x61, 0x6B, 0x65, 0x5F, 0x68, 0x61,
                   0x6E, 0x64, 0x5F, 0x72, 0x65, 0x73, 0x70};
  std::stringstream ss;
  for (auto it : msg_name)
    ss << it;   
  std::string val;
  ss >> val;             
  header->__set_msg_name(val);

  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
  binary_protocol proto(buf.get());

  header->write(&proto);

  // char* tmp_buf =  buf->get_buf();
  // x/10x tmp_buf
  // 0xdadc50: 0xe1010008  0x0b97da0d1  0x00000200  0x68730f00
  // 0xdadc60: 0x5f656b61  0x646e6168  0x7365725f  0x00007f70
  
  BOOST_TEST(header->msg_name == "shake_hand_resp");
}
