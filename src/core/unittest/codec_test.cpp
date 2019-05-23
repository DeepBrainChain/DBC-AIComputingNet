/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : codec_test.cpp
* description       : include decoder.h/encoder.h/length_field_frame_decoder.h
*                     message_to_byte_encoder.h
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "message_to_byte_encoder.h"

using namespace matrix::core;

BOOST_AUTO_TEST_CASE(test_message_to_byte_encoder)
{
  std::shared_ptr<message_to_byte_encoder> encoder = std::make_shared<message_to_byte_encoder>();
  std::shared_ptr<message> msg = std::make_shared<message>(); 
  std::shared_ptr<msg_base> content = std::make_shared<msg_base>();
  // message must set content,now message don not judge content is nullptr
  msg->set_content(content);

  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
  channel_handler_context ctx;
  encode_status ret = encoder->encode(ctx, *(msg.get()), *(buf.get()));
  BOOST_TEST(ret == ENCODE_SUCCESS);
}
