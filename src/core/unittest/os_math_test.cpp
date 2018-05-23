// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "os_math.h"

using namespace std;


BOOST_AUTO_TEST_CASE(test_sqrt, * utf::tolerance(float(0.001))){

    BOOST_TEST( my_sqrt(4) == float(2.0));
    BOOST_TEST( my_sqrt(0) == float(0.0));
//    BOOST_TEST( my_sqrt(-1) == float(-1.0));
//    BOOST_TEST( my_sqrt(-4) == float(-2.0));


    // Boundary
    BOOST_TEST( my_sqrt(1.8E-38) == float(0.0));
    BOOST_TEST(my_sqrt(3.4E+38) == float(1.8439e+19));
}
