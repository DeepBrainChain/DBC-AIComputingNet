
// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for dbc core"

// dbc use dynamic boost lib in mac os
#if defined MAC_OSX
//#define BOOST_TEST_DYN_LINK
#endif

#include <boost/test/unit_test.hpp>
