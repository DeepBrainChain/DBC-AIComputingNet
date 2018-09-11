/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : ip_validator_test.h
* description       :
* date              : 2018/8/1
* author            : Taz Zhang
**********************************************************************************/
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <string>

#include <conf/validator/ip_validator.h>




using namespace matrix::core;
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test_ip_validator) {
    bool isOK;
    std::string valid_ip = "192.168.1.1";
    std::string invalid_ip1 = "192.168.1.x";
    std::string invalid_ip2 = "1922.168.1.1";
    std::string invalid_ip3 = "192";
    std::string invalid_ip4 = "0.0.0";
    std::string invalid_ip5 = "0.0.0.0.0";
    std::string invalid_ip6 = "192. 168.1.1 ";

    ip_validator ip_vdr;

    variable_value val;
    val.value() = valid_ip;

    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == true, "ip_vdr.validate( val)");

    val.value() = invalid_ip1;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip1)");


    val.value() = invalid_ip2;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip2)");


    val.value() = invalid_ip3;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip3)");

    val.value() = invalid_ip4;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip4)");


    val.value() = invalid_ip5;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip5)");

    val.value() = invalid_ip6;
    isOK = ip_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "ip_vdr.validate( invalid_ip6)");
}