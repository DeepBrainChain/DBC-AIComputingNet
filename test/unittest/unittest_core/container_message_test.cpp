/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : container_message_test.cpp
* description       : test container_mesage.cpp
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

//#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "container_message.h"
#include "document.h"

using namespace matrix::core;

BOOST_AUTO_TEST_CASE(test_container_message_to_string)
{
  std::shared_ptr<container_config> container_config_ptr = std::make_shared<container_config>();
  container_config_ptr->host_name = "test";
  std::string container_string = container_config_ptr->to_string();

  rapidjson::Document doc;
  BOOST_TEST(!(doc.Parse<0>(container_string.c_str()).HasParseError()));

  rapidjson::Value& host_name = doc["Hostname"];
  std::string host_string = host_name.GetString();
  BOOST_TEST(host_string == "test");
}
