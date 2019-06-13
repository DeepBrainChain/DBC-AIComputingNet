
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

// rapid json head files
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

// boost property tree head files
#include <boost/property_tree/json_parser.hpp>


#include <iostream>
#include <memory>

using namespace std;
using namespace rapidjson;

BOOST_AUTO_TEST_CASE(test_rapidjson_parse_gpu_str)
{
    string gpu = "{\"num\":\"1\", \"driver\":\"410.78\", \"cuda\":\"10.0\", \"p2p\":\"nok\", \"gpus\":[{\"id\":\"0\", \"type\":\"GeForce940MX\", \"mem\":\"2004 MiB\", \"bandwidth\":\"1682.8 MB/s\"}]}";

    Document doc;

    Document doc2;

    BOOST_TEST(doc2.Parse<0>(gpu.c_str()).HasParseError() != true);

    BOOST_TEST(doc2["num"].GetString() == "1");

    Value gpu_value = Value(doc2, doc.GetAllocator());

    Value root(kObjectType);
    root.AddMember("gpu", gpu_value, doc.GetAllocator());

    BOOST_TEST(root["gpu"]["num"].GetString() == "1");

}

BOOST_AUTO_TEST_CASE(test_rapidjson_parse_gpu_usage_str)
{
    string gpu_usage="[ {\"mem\":\"10%\", \"gpu\":\"20%\"}, {\"mem\":\"15%\", \"gpu\":\"25%\"} ]";

    Document doc;
    BOOST_TEST(doc.Parse<0>(gpu_usage.c_str()).HasParseError() != true);

    BOOST_TEST(doc[0]["mem"].GetString() == "10%");
}




BOOST_AUTO_TEST_CASE(test_rapidjson_encode_gpu_usage_str)
{
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    rapidjson::Value root(rapidjson::kObjectType);
    root.AddMember("error_code", 0, allocator);

    std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
    root.Accept(writer);

    std::string strJSON(buffer->GetString());

    cout<<strJSON<<endl;
}


BOOST_AUTO_TEST_CASE(test_boost_decode_gpu_isolation_str)
{
    std::stringstream ss;
    ss << "{\"env\": {\"NVIDIA_VISIBLE_DEVICES\": \"0\"}}";
    boost::property_tree::ptree pt;

    try
    {
        boost::property_tree::read_json(ss, pt);
        BOOST_TEST(pt.get<std::string>("env.NVIDIA_VISIBLE_DEVICES") == "0");
    }
    catch (exception & e)
    {
        cout << e.what() << endl;
    }
}


BOOST_AUTO_TEST_CASE(test_boost_encode_gpu_isolation_str)
{
    using namespace boost::property_tree;

    ptree pt;
    ptree env;

    env.put("NVIDIA_VISIBLE_DEVICES", "0");
    pt.put_child("env", env);

    stringstream ss;
    write_json(ss, pt);

    cout << ss.str() << endl;
}