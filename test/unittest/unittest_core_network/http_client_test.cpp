/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : http_client_test.cpp
* description       :
* date              : 2018/8/1
* author            : Taz Zhang
**********************************************************************************/
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <string>
#include <iostream>
#include "http_client.h"

using namespace matrix::core;
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test_http_client) {

    int32_t ret;
    std::string url = "http://www.baidu.com";
    std::string crt="";
    http_client _http_client(url,crt);

    std::string endpoint = "http://www.baidu.com";
    kvs headers;
    http_response resp;

    try {
        ret = _http_client.get(endpoint, headers, resp);
        BOOST_TEST_CHECK(E_SUCCESS == ret, "_http_client.get ok");
    }
    catch (const std::exception &e) {
        LOG_ERROR << "_http_client.get(endpoint, headers, resp) caught exception : " << e.what();
        BOOST_TEST_CHECK(1 == 0, "_http_client.get failed");
    }


    std::string req_content = "";
    ret = _http_client.post(endpoint, headers, req_content, resp);

    try {
        ret = _http_client.post(endpoint, headers, req_content, resp);
        BOOST_TEST_CHECK(E_SUCCESS == ret, "_http_client.post ok");
    }
    catch (const std::exception &e) {
        LOG_ERROR << "_http_client.post( endpoint,headers,req_content,resp) caught exception : " << e.what();
        BOOST_TEST_CHECK(1 == 0, "_http_client.post failed");
    }

}