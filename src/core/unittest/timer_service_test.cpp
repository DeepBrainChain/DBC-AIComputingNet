/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : timer_service_test.cpp
* description       : test files in timer_service directory
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "timer_matrix_manager.h"
#include "timer_manager.h"
#include "pc_service_module.h"
#include "topic_manager.h"
#include "server.h"

using namespace matrix::core;

#define TIMER_NAME_TEST "test_timer_name"

static uint32_t count_test = 0;
static int32_t on_timer_check_result(std::shared_ptr<matrix::core::core_timer> timer)
{
  (void)timer;
  count_test++;
  return E_SUCCESS;
}

// mock service module
class mock_service_module : public service_module
{
public:
  uint32_t set_timer_invokers(int32_t period, uint64_t repeat_times, const std::string & session_id)
  {
    m_timer_invokers[TIMER_NAME_TEST] = std::bind(on_timer_check_result, std::placeholders::_1);
    return add_timer(TIMER_NAME_TEST, period, repeat_times, "");	
  }

  void remove_timer_mock(uint32_t timer_id)
  {
    remove_timer(timer_id);
  }
};

// prepare startup environment
static int32_t test_environment_prepare()
{
  std::shared_ptr<matrix::core::module> mdl(nullptr);
  variables_map vm;

  // topic manager
  mdl = std::dynamic_pointer_cast<module>(std::make_shared<topic_manager>());
  g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
  int32_t ret = mdl->init(vm);
  if (ret != E_SUCCESS) {
    return ret;
  }
  mdl->start();

  // timer manager
  mdl = std::dynamic_pointer_cast<module>(std::make_shared<timer_matrix_manager>());
  g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
  ret = mdl->init(vm);
  if (ret != E_SUCCESS) {
    return ret;
  }
  mdl->start();  

  count_test = 0;

  return ret;
}

// restore environment
static int32_t test_environment_destroy()
{
  int32_t ret = g_server->get_module_manager()->stop();
  ret |= g_server->get_module_manager()->exit();
 
  return ret;
}

BOOST_AUTO_TEST_CASE(test_timer_service)
{
  int32_t ret = test_environment_prepare();
  BOOST_TEST(ret == E_SUCCESS);

  bpo::variables_map options;
  std::shared_ptr<mock_service_module> service_module_ptr = std::make_shared<mock_service_module>();
  ret = service_module_ptr->init(options);
  // mock test
  (void)service_module_ptr->set_timer_invokers(100, ULLONG_MAX, "");

  ret |= service_module_ptr->start();
  BOOST_TEST(ret == E_SUCCESS);

// test the influence of timer message for service message,
// no timer  add timer
// 114479     114337
// 118142     104414
// 113895     196441
// 114794     104477
/*
 std::shared_ptr<message> msg = std::make_shared<message>();
   int i = 0;
  while (true) {
    i++;
    ret = service_module_ptr->send(msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (ret == E_MSG_QUEUE_FULL)
      break;
  }

  cout << "message data max cout=" << i << endl;
*/
  // delay 1s
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));

  ret = service_module_ptr->stop();
  ret |= service_module_ptr->exit();

  ret |= test_environment_destroy();

  BOOST_TEST(count_test == 3000/100);
}

BOOST_AUTO_TEST_CASE(test_timer_service_10ms)
{
  int32_t ret = test_environment_prepare();
  BOOST_TEST(ret == E_SUCCESS);

  bpo::variables_map options;
  std::shared_ptr<mock_service_module> service_module_ptr = std::make_shared<mock_service_module>();
  ret = service_module_ptr->init(options);
  // period < 100
  uint32_t timer_ret = service_module_ptr->set_timer_invokers(10, ULLONG_MAX, "");
  BOOST_TEST(timer_ret == (uint32_t)INVALID_TIMER_ID);

  // repeat time < 1
  timer_ret = service_module_ptr->set_timer_invokers(100, 0, "");
  BOOST_TEST(timer_ret == (uint32_t)INVALID_TIMER_ID);

  ret = service_module_ptr->stop();
  ret |= service_module_ptr->exit();

  ret |= test_environment_destroy();
  BOOST_TEST(ret == E_SUCCESS);
}

BOOST_AUTO_TEST_CASE(test_timer_service_repeat)
{
  int32_t ret = test_environment_prepare();
  BOOST_TEST(ret == E_SUCCESS);

  bpo::variables_map options;
  std::shared_ptr<mock_service_module> service_module_ptr = std::make_shared<mock_service_module>();
  ret = service_module_ptr->init(options);

  uint32_t timer_ret = service_module_ptr->set_timer_invokers(100, 1, "");
  BOOST_CHECK(timer_ret != (uint32_t)E_DEFAULT);

  ret |= service_module_ptr->start();
  BOOST_CHECK_EQUAL(ret, E_SUCCESS);

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));

  ret = service_module_ptr->stop();
  ret |= service_module_ptr->exit();

  ret |= test_environment_destroy();

  BOOST_TEST(count_test == 1);
}
