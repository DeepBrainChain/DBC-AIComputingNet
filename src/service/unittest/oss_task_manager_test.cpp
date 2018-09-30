// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include <iostream>
#include <memory>

#include "oss_task_manager.h"

using namespace std;
using namespace ai::dbc;

BOOST_AUTO_TEST_CASE(test_load_oss_config)
{
    std::shared_ptr<container_client> m_container_client = std::make_shared<container_client>("127.0.0.1", 33307);
   	std::shared_ptr<idle_task_scheduling>	m_idle_task_ptr = std::make_shared<idle_task_scheduling>(m_container_client);
    oss_task_manager s(m_idle_task_ptr);
    int ret = s->load_oss_config();
    BOOST_TEST(ret == 0);
}

BOOST_AUTO_TEST_CASE(test_oss_init)
{
   	std::shared_ptr<container_client> m_container_client = std::make_shared<container_client>("127.0.0.1", 33307);
   	std::shared_ptr<idle_task_scheduling>	m_idle_task_ptr = std::make_shared<idle_task_scheduling>(m_container_client);
    oss_task_manager s(m_idle_task_ptr);
    auto ret = s.init();
    BOOST_TEST(ret == 0);
}


