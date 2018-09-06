/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : pc_service_module_test.cpp
* description       : 
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "server.h"
#include "service_module.h"
#include "topic_manager.h"

using namespace matrix::core;
namespace bpo = boost::program_options;

// prepare startup environment
static int32_t test_environment_prepare()
{
  std::shared_ptr<matrix::core::module> mdl(nullptr);
  variables_map vm;

  // topic_manager
  mdl = std::dynamic_pointer_cast<module>(std::make_shared<topic_manager>());
  g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
  int32_t ret = mdl->init(vm);
  if (ret != E_SUCCESS) {
    return ret;
  }
  mdl->start();

  return ret;
}

// restore environment
static int32_t test_environment_destroy()
{
  int32_t ret = g_server->get_module_manager()->stop();
  ret |= g_server->get_module_manager()->exit();
 
  return ret;
}

BOOST_AUTO_TEST_CASE(test_service_module)
{
  int32_t ret = test_environment_prepare();
  BOOST_TEST(ret == E_SUCCESS);

  bpo::variables_map options;
  std::shared_ptr<service_module> service_module_ptr = std::make_shared<service_module>();
  ret = service_module_ptr->init(options);
  ret |= service_module_ptr->start();
  BOOST_TEST(ret == E_SUCCESS);

/*  // delay 1s
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::shared_ptr<message> msg = std::make_shared<message>();

 performance test result:(send max message below)
timeout    timeout  timeout
100ms       500ms    1s
114962      141835   118688
114443      115991   107560
114275      108108   115287
126145      109503   112544
115960      116537   165758
114629      119305   140946
*/
/*
  int i = 0;
  while (true) {
    i++;
    ret = service_module_ptr->send(msg);
    if (ret == E_MSG_QUEUE_FULL)
      break;
  }

  cout << "message data max cout=" << i << endl;
*/  
  // delay 1s,wait thread schedule,process message
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  ret = service_module_ptr->stop();
  ret |= service_module_ptr->exit();
  ret |= test_environment_destroy();
  BOOST_TEST(ret == E_SUCCESS);
}
