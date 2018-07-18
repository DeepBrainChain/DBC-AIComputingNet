
// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "common/util.h"
using namespace matrix::core;

BOOST_AUTO_TEST_CASE(test_rtrim_one_whitespace) {

    std::string s=" h ello ";
    auto t=string_util::rtrim(s, ' ');
    BOOST_TEST(t==" h ello");
}

BOOST_AUTO_TEST_CASE(test_rtrim_one_cr) {

    std::string s="hello \n";
    auto t=string_util::rtrim(s, '\n');
    BOOST_TEST(t=="hello ");
}

BOOST_AUTO_TEST_CASE(test_rtrim_two_cr) {

    std::string s="hello\n \n\n";
    auto t=string_util::rtrim(s, '\n');
    BOOST_TEST(t=="hello\n ");
}

BOOST_AUTO_TEST_CASE(test_rtrim_empty) {

    std::string s="";
    auto t=string_util::rtrim(s, '\n');
    BOOST_TEST(t=="");
}
