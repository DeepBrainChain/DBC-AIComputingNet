/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : filter_test.cpp
* description       : 
* date              : 2018/7/1
* author            : Jimmy Kuang
**********************************************************************************/
// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "filter/simple_expression.h"
#include "filter/fulltext.h"

using namespace boost::unit_test;
using namespace std;
using namespace matrix::core;

BOOST_AUTO_TEST_CASE(test_simple_expression_condition_equal_match) {
    string s = "gpu=1";
    condition c;
    c.set(s);
    bool r = c.evaluate("gpu","1");
    BOOST_TEST(r == true);
}

BOOST_AUTO_TEST_CASE(test_simple_expression_condition_equal_not_match) {
    string s = "cpu=2";
    condition c;
    c.set(s);
    bool r = c.evaluate("cpu","1");
    BOOST_TEST(r == false);
}

BOOST_AUTO_TEST_CASE(test_simple_expression_condition_gt_match) {
    string s = "gpu>1";
    condition c;
    c.set(s);
    bool r = c.evaluate("gpu","2");
    BOOST_TEST(r == true);
}

BOOST_AUTO_TEST_CASE(test_simple_expression_condition_gt_not_match) {
    string s = "gpu>3";
    condition c;
    c.set(s);
    bool r = c.evaluate("gpu","2");
    BOOST_TEST(r == false);
}

BOOST_AUTO_TEST_CASE(test_simple_expression_condition_lt_match) {
    string s = "gpu_num<3";
    condition c;
    c.set(s);
    bool r = c.evaluate("gpu_num","2");
    BOOST_TEST(r == true);
}


BOOST_AUTO_TEST_CASE(test_simple_expression_condition_lt_not_match) {
    string s = "gpu_num<3";
    condition c;
    c.set(s);
    bool r = c.evaluate("gpu_num","4");
    BOOST_TEST(r == false);
}

BOOST_AUTO_TEST_CASE(test_simple_expression_expression_equal_match) {
    string s = "gpu=1";

    expression e(s);
    map<string,string> kv;
    kv["gpu"]="1";
    bool r = e.evaluate(kv);
    BOOST_TEST(r == true);

}

BOOST_AUTO_TEST_CASE(test_simple_expression_match_two_conditions) {
    string s = "gpu_num=1 and gpu_pw>5";

    expression e(s);
    BOOST_TEST(e.size() == 2);

    map<string,string> kv;
    kv["gpu_num"]="1";
    kv["gpu_pw"]="6";

    bool r = e.evaluate(kv);
    BOOST_TEST(r == true);

}


BOOST_AUTO_TEST_CASE(test_simple_expression_not_match_two_conditions) {
    string s = "gpu_num=1 and gpu_pw>5";

    expression e(s);
    auto n = e.size();
    BOOST_TEST(n == 2);

    map<string,string> kv;
    kv["gpu_num"]="1";
    kv["gpu_pw"]="1";

    bool r = e.evaluate(kv);
    BOOST_TEST(r == false);

}

BOOST_AUTO_TEST_CASE(test_simple_expression_invalid_case1) {
    string s = "gpu_num=1 and  ";

    expression e(s);

    BOOST_TEST(e.size() == 1);

}

BOOST_AUTO_TEST_CASE(test_simple_expression_invalid_case2) {
    string s = "gpu_num=";

    expression e(s);

    map<string,string> kv;
    kv["gpu_num"]="1";

    bool r = e.evaluate(kv);
    BOOST_TEST(r == false);
}

//BOOST_AUTO_TEST_CASE(test_simple_expression_double_quote) {
//    string s = "\"gpu_num=1\"";
//
//    expression e(s);
//
//    map<string,string> kv;
//    kv["gpu_num"]="1";
//
//    bool r = e.evaluate(kv);
//    BOOST_TEST(r == true);
//}


BOOST_AUTO_TEST_CASE(test_simple_expression_fulltext_match_and_not_case_sensitive) {
    string s = "2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT     amzaon's pc1    ai_training             idle        1 * TeslaK80";


    vector<string> keywords(2);
    keywords[0] = "tesla";
    keywords[1] = "idle";

    bool r = fulltext::search(s,keywords);
    BOOST_TEST(r == true);
}


BOOST_AUTO_TEST_CASE(test_simple_expression_fulltext_not_match) {
    string s = "2gfpp3MAB4ACimTtKJtNUj2cGzaZhcwcg3NuNUmgZTT     amzaon's pc1    ai_training             idle        1 * TeslaK80";


    vector<string> keywords(2);
    keywords[0] = "geforce";
    keywords[1] = "idle";

    bool r = fulltext::search(s,keywords);
    BOOST_TEST(r == false);
}