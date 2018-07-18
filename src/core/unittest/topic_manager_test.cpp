// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


using namespace boost::unit_test;

#include "topic_manager.h"
#include <string>

BOOST_AUTO_TEST_CASE(service_bus_test) {

    matrix::core::topic_manager tm;

    std::string out;

    tm.subscribe("sport", [&](std::string s) { out = s; });

    tm.publish<void, std::string>("sport", "soccer");

//    tm.publish<void, std::string>("music", "hi U2");
    BOOST_TEST(out=="soccer");
}


//BOOST_AUTO_TEST_CASE(raw_string_test) {
//
//    std::string r = R"(
//        #!/bin/bash
//function get {
//    attr=$1
//
//    case "$attr" in
//    "gpu")
//    if ! which nvidia-smi>/dev/null; then echo "no gpu"; fi
//            nvidia-smi -L | awk -F "(" '{print $1}'
//    ;;
//
//    "cpu")
//    cat /proc/cpuinfo |grep "model name"
//    ;;
//    )";
//
//    std::cout << r << "\n";
//}