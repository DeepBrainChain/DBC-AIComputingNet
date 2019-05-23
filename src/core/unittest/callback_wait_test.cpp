/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : callback_wait_test.cpp
* description       : 
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "callback_wait.h"
#include <mutex>
#include <iostream>
#include <thread>

using namespace matrix::core;

static uint call_test_count = 0; 
static std::mutex test_mutex;

void call_test_sub()
{
  std::lock_guard<std::mutex> lock(test_mutex);
  call_test_count++;
}

constexpr uint32_t THREAD_NUM = 10;

BOOST_AUTO_TEST_CASE(test_callback_wait)
{
  std::shared_ptr<callback_wait<>> callback_wait_var = std::make_shared<callback_wait<>>(call_test_sub);
  callback_wait_var->reset();

  auto fun = [=](uint32_t time) { callback_wait_var->wait_for(std::chrono::milliseconds(time)); };
  std::thread ts[THREAD_NUM];
  for(uint32_t i = 0; i < THREAD_NUM; i++)
  {
    ts[i] = std::thread(fun, 3000+ i * 5);
  }

  // delay 1s
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  callback_wait_var->set();
 
  for(uint32_t i = 0; i < THREAD_NUM; i++)
  {
    ts[i].join();
  }

  BOOST_TEST(call_test_count == THREAD_NUM);
}
