// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include <iostream>
#include <memory>

#include "service_info_collection.h"

using namespace matrix::service_core;
using namespace service::misc;
using namespace std;

BOOST_AUTO_TEST_CASE(test_get_change_set_size_0)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    auto cs = sc.get_change_set();
    BOOST_TEST(cs.size() == 0);
}

BOOST_AUTO_TEST_CASE(test_get_change_set_size_1)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    node_service_info nsi;
    nsi.__set_name("pc1");
    nsi.__set_time_stamp(1);
    vector<string> v;
    v.push_back("ai_training");
    nsi.__set_service_list(v);

    sc.add("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp", nsi);

    auto cs = sc.get_change_set();
    BOOST_TEST(cs.size() == 1);
}

BOOST_AUTO_TEST_CASE(test_get_change_set_size_128)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    string id_base= "2gfpp3MAB41g5L9wRh6RF5PcyMEuJz53g1xckU4S";
    for(int i=0; i< 128; i++)
    {
        node_service_info nsi;
        nsi.__set_name("pc"+std::to_string(i));
        nsi.__set_time_stamp(0);

        vector<string> v;
        v.push_back("ai_training");
        nsi.__set_service_list(v);

        char str[4];
        snprintf (str, 4, "%03d", i);
        //printf("%s\n", str);
        sc.add(id_base + std::string(str), nsi);
    }

    auto cs = sc.get_change_set();

    BOOST_TEST(cs.size() == 32);
}


BOOST_AUTO_TEST_CASE(test_get_change_set_no_overlap)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    string id_base= "2gfpp3MAB41g5L9wRh6RF5PcyMEuJz53g1xckU4S";
    for(int i=0; i< 128; i++)
    {
        node_service_info nsi;
        nsi.__set_name("pc"+std::to_string(i));
        nsi.__set_time_stamp(0);

        vector<string> v;
        v.push_back("ai_training");
        nsi.__set_service_list(v);

        char str[4];
        snprintf (str, 4, "%03d", i);
        //printf("%s\n", str);
        sc.add(id_base + std::string(str), nsi);
    }

    auto cs1 = sc.get_change_set();
    BOOST_TEST(cs1.size() == 32);

    auto cs2 = sc.get_change_set();

    BOOST_TEST(cs2.count(id_base+"032")==1);
    BOOST_TEST(cs2.count(id_base+"063")==1);

}


BOOST_AUTO_TEST_CASE(test_get_change_set_loop)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    string id_base= "2gfpp3MAB41g5L9wRh6RF5PcyMEuJz53g1xckU4S";
    for(int i=0; i< 40; i++)
    {
        node_service_info nsi;
        nsi.__set_name("pc"+std::to_string(i));
        nsi.__set_time_stamp(0);

        vector<string> v;
        v.push_back("ai_training");
        nsi.__set_service_list(v);

        char str[4];
        snprintf (str, 4, "%03d", i);
        //printf("%s\n", str);
        sc.add(id_base + std::string(str), nsi);
    }

    auto cs1 = sc.get_change_set();
    BOOST_TEST(cs1.size() == 32);

    auto cs2 = sc.get_change_set();


    BOOST_TEST(cs2.count(id_base+"032")==1);
    BOOST_TEST(cs2.count(id_base+"023")==1);

}



BOOST_AUTO_TEST_CASE(test_remove_dead_node)
{
    service_info_collection sc;
    sc.init("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp");

    node_service_info nsi;
    nsi.__set_name("pc1");
    nsi.__set_time_stamp(1);
    vector<string> v;
    v.push_back("ai_training");
    nsi.__set_service_list(v);

    sc.add("2gfpp3MAB3xSZjSnTMYu13Ccwg9Gb83kk2irByb9aVp", nsi);

    sc.remove_unlived_nodes(5);
    auto cs = sc.get_change_set();
    BOOST_TEST(cs.size() == 0);
}