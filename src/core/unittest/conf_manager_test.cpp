/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : conf_manager_test.cpp
* description       : test conf_manager
* date              : 2018/7/30
* author            : tower
**********************************************************************************/

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <iostream>
#include "conf_manager.h"
#include "env_manager.h"
#include "server.h"

class mock_conf_manager : public conf_manager
{
  public:
    int32_t parse_local_conf_mock() { return parse_local_conf(); }

    int32_t parse_node_dat_mock() { return parse_node_dat(); }

    int32_t init_params_mock() { return init_params(); }

    void init_net_flag_mock() { init_net_flag(); }

  private:

};

// prepare startup environment
static int32_t test_environment_prepare()
{
  std::shared_ptr<matrix::core::module> mdl(nullptr);
  variables_map vm;

  mdl = std::dynamic_pointer_cast<module>(std::make_shared<env_manager>());
  g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
  int32_t ret = mdl->init(vm);
  if (ret != E_SUCCESS)
  {
    return ret;
  }
  mdl->start();

  mdl = std::dynamic_pointer_cast<module>(std::make_shared<conf_manager>());
  g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
  ret = mdl->init(vm);
  if (ret != E_SUCCESS)
  {
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

BOOST_AUTO_TEST_CASE(test_conf_manager_local_conf)
{
  int32_t ret = test_environment_prepare();
  BOOST_TEST(ret == E_SUCCESS);

  std::shared_ptr<mock_conf_manager> conf_manager = std::make_shared<mock_conf_manager>();

  ret = conf_manager->parse_local_conf_mock();

  BOOST_TEST(ret == E_SUCCESS);

  ret = conf_manager->parse_node_dat_mock();

  BOOST_TEST(ret == E_SUCCESS);

  ret = conf_manager->init_params_mock();
  BOOST_TEST(ret == E_SUCCESS);

  ret = test_environment_destroy();
  BOOST_TEST(ret == E_SUCCESS);
}
