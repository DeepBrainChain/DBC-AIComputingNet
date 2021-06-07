// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include <iostream>
#include <memory>

#include "node_info_collection.h"
#include "core/common/time_util.h"

//using namespace matrix::service_core;
using namespace service::misc;
using namespace std;

BOOST_AUTO_TEST_CASE(test_gpu_usage_in_total)
{
    node_info_collection nc;

    // null
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("N/A"));

    std::string s = "gpu: 0 %\nmem: 0 %\ngpu: 0 %\nmem: 0 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("0 %"));

    s = "gpu: 10 %\nmem: 1 %\ngpu: 0 %\nmem: 0 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("5 %"));

    s = "gpu: 100 %\nmem: 1 %\ngpu: 100 %\nmem: 0 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("100 %"));

    s = "gpu: 0 %\nmem: 1 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("0 %"));

    s = "gpu: 0.1 %\nmem: 1 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("0 %"));

    s = "gpu: .1 %\nmem: 1 %\n";
    nc.set("gpu_usage", s);
    BOOST_TEST(nc.get_gpu_usage_in_total() == std::string("0 %"));

}

BOOST_AUTO_TEST_CASE(test_node_startup_time)
{
    node_info_collection nc;
    BOOST_TEST(nc.get_node_startup_time() == 0);

    nc.set_node_startup_time();
    BOOST_TEST(nc.get_node_startup_time() != 0);
    std::cout<<"node startup time: " << matrix::core::time_util::time_2_str(nc.get_node_startup_time())<<endl;
}


BOOST_AUTO_TEST_CASE(test_read_node_info_conf)
{
    node_info_collection nc;

    nc.read_node_static_info("../src/service/unittest/dbc_node_info.conf");

    BOOST_TEST(nc.get("network_dl") == std::string("10.71 Mbit/s"));
    BOOST_TEST(nc.get("network_ul") == std::string("14.05 Mbit/s"));

    nc.get_gpu_short_desc();
}