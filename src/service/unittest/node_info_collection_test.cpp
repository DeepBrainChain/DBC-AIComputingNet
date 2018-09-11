// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include <iostream>
#include <memory>

#include "node_info_collection.h"

//using namespace matrix::service_core;
using namespace service::misc;
using namespace std;

BOOST_AUTO_TEST_CASE(test_gpu_usage_in_total)
{
    node_info_collection nc;

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
