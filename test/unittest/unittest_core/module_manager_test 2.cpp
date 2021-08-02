
#include <boost/test/unit_test.hpp>


#include "module_manager.h"
#include "error.h"

using namespace boost::unit_test;
using namespace std;



BOOST_AUTO_TEST_CASE(test_module_manager) {

    matrix::core::module_manager mg;

    bpo::variables_map v;

    int32_t r = mg.init(v);
    matrix::core::module m1;
    auto s = std::make_shared<matrix::core::module>(m1);

    mg.add_module("m1", s);


    BOOST_TEST(r == E_SUCCESS);

    auto t = mg.get("m1");
//    BOOST_TEST(t.get()!= nullptr);
}


