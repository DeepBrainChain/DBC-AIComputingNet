/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   common_def.cpp
* description    :   common definition
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#include "common_def.h"
#include "common.h"
#include <boost/asio.hpp>

using namespace boost::asio;

void assert_failed(uint8_t* file, uint32_t line)
{ 
  while (1)
  {

  }
}

std::string DEFAULT_STRING("");
std::string DEFAULT_LOCAL_IP("0.0.0.0");
std::string DEFAULT_LOOPBACK_IP("127.0.0.1");
std::string DEFAULT_IP_V4(ip::address_v4::any().to_string());
std::string DEFAULT_IP_V6(ip::address_v6::any().to_string());
std::vector<std::string> DEFAULT_VECTOR;

