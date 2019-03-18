// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include <iostream>

#include "common/util.h"
#include "gpu_pool.h"

using namespace ai::dbc;
using namespace matrix::core;



BOOST_AUTO_TEST_CASE(test_read_gpu_info)
{

    std::string path = "../src/service/unittest/gpus";

    gpu_pool pool;
    gpu_pool_helper::update_gpu_from_proc(pool, path);
    std::cout << pool.toString();
}

BOOST_AUTO_TEST_CASE(test_gpu_pool)
{
    gpu_pool pool;

    std::set<gpu> gpus;
    gpus.insert(gpu(0, "Geforce1080Ti"));
    gpus.insert(gpu(1, "Geforce1080Ti"));
    gpus.insert(gpu(2, "Geforce1080Ti"));
    gpus.insert(gpu(3, "Geforce1080Ti"));

    pool.init(gpus);

    std::cout << pool.toString();

    auto a = pool.allocate(0);
    BOOST_TEST(a->id() == 0);
    BOOST_TEST(a->type() == "Geforce1080Ti");

    pool.allocate(3);
    pool.free(0);
    BOOST_TEST(pool.count() == 4);
    BOOST_TEST(pool.count_free() == 3);
    BOOST_TEST(pool.count_busy() == 1);

    pool.free(0);

    BOOST_TEST(pool.allocate(3)==nullptr);

    pool.free(3);
    pool.allocate("all");

    BOOST_TEST(pool.count_free()==0);


    pool.free("1,2");
    BOOST_TEST(pool.count_free()==2);

    pool.allocate("1");
    BOOST_TEST(pool.count_free()==1);
    BOOST_TEST(pool.count_busy()==3);

    pool.allocate("1,2,3");
    BOOST_TEST(pool.count_free()==1);

    pool.free("all");
    BOOST_TEST(pool.count_free()==4);
    BOOST_TEST(pool.count_busy()==0);


    std::cout << pool.toString();

}