/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : port_validator_test.h
* descrporttion       :
* date              : 2018/8/1
* author            : Taz Zhang
**********************************************************************************/
//#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <string>
#include <iostream>

#include "log.h"
#include <conf/validator/port_validator.h>




using namespace matrix::core;
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test_port_validator) {
    bool isOK;
    std::string valid_port = "1026";
    std::string invalid_port1 = "";
    std::string invalid_port2 = "q";
    std::string invalid_port3 = "1 223";
    std::string invalid_port4 = " 2323";
    std::string invalid_port5 = "23a3";
    std::string invalid_port6 = "111111111111";

    port_validator port_vdr;

    variable_value val;
    val.value() = valid_port;

    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == true, "port_vdr.validate( val)");

    val.value() = invalid_port1;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port1)");


    val.value() = invalid_port2;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port2)");


    val.value() = invalid_port3;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port3)");

    val.value() = invalid_port4;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port4)");


    val.value() = invalid_port5;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port5)");

    val.value() = invalid_port6;
    isOK = port_vdr.validate(val);
    BOOST_TEST_CHECK(isOK == false, "port_vdr.validate( invalid_port6)");
}