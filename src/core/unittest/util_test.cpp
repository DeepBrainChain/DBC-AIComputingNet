
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

BOOST_AUTO_TEST_CASE(test_split_delim_empty) {

    std::string s = "abcd";
    std::string delim = "";
    std::vector<std::string> vec;
    string_util::split(s, delim, vec);

    BOOST_TEST(vec[0] == "abcd");
}

BOOST_AUTO_TEST_CASE(test_split_string_one) {

    std::string s = "";
    std::string delim = "";
    std::vector<std::string> vec;
    string_util::split(s, delim, vec);

    BOOST_TEST(vec.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_split_char_one) {

    char s[] = "abaca";
    char delim = 'a';
    int argc = 5;
    char* argv[5];
    string_util::split(&s[0], delim, argc, &argv[0]);

    BOOST_TEST(argc == 2);
}
