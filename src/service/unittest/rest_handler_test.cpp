
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <iostream>
#include <memory>


using namespace std;
using namespace rapidjson;

BOOST_AUTO_TEST_CASE(test_parse_gpu_str)
{
    string gpu="{\"num\":\"1\", \"driver\":\"410.78\", \"cuda\":\"10.0\", \"p2p\":\"nok\", \"gpus\":[{\"id\":\"0\", \"type\":\"GeForce940MX\", \"mem\":\"2004 MiB\", \"bandwidth\":\"1682.8 MB/s\"}]}";

    Document doc;

    Document doc2;

    BOOST_TEST(doc2.Parse<0>(gpu.c_str()).HasParseError() != true);

    BOOST_TEST(doc2["num"].GetString() == "1");

    Value gpu_value = Value(doc2, doc.GetAllocator());

    Value root(kObjectType);
    root.AddMember("gpu",gpu_value, doc.GetAllocator());

    BOOST_TEST(root["gpu"]["num"].GetString() == "1");

}
