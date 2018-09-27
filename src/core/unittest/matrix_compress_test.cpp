// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "compress/matrix_compress.h"
#include <iostream>
#include <memory>

#include "snappy.h"
#include "codec/protocol.h"

using namespace matrix::core;
using namespace std;

void GetContents(const std::string& filename, std::string* data)
{
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == NULL)
    {
        perror(filename.c_str());
        return;
    }

    data->clear();
    while (!feof(fp))
    {
        char buf[4096];
        size_t ret = fread(buf, 1, 4096, fp);
        if (ret == 0 && ferror(fp))
        {
            perror("fread");
            return;
        }
        data->append(std::string(buf, ret));
    }

    fclose(fp);
}

void SetContents(
        const std::string& filename, const std::string& str)
{
    FILE *fp = fopen(filename.c_str(), "wb");
    if (fp == NULL)
    {
        perror(filename.c_str());
        return;
    }

    int ret = fwrite(str.data(), str.size(), 1, fp);
    if (ret != 1)
    {
        perror("fwrite");
        return;
    }

    fclose(fp);
}

//BOOST_AUTO_TEST_CASE(test_snappy_file)
//{
//    string fn="/tmp/srv_broadcast";
//    string input;
//    GetContents(fn,&input);
//
//    string output;
//    snappy::Compress(input.data(), input.size(), &output);
//    cout << "input size:" << input.size() << " output size:"
//         << output.size() << endl;
//
//    SetContents(string(fn).append(".comp"), output);
//
//}

//BOOST_AUTO_TEST_CASE(test_snappy)
//{
////    string input = std::string((char*)text, sizeof(text));
//    string input = "hello wolrd\nhello wolrd\nhello wolrd\nhello wolrd\n";
//    string output;
//    snappy::Compress(input.data(), input.size(), &output);
//
//    string output_uncom;
//    snappy::Uncompress(output.data(), output.size(), &output_uncom);
//    if (input == output_uncom) {
//        cout << "Equal" << endl;
//    } else {
//        cout << "ERROR: not equal" << endl;
//    }
//
//}

BOOST_AUTO_TEST_CASE(test_matrix_compress)
{
    string input = "hello wolrd\nhello wolrd\nhello wolrd\nhello wolrd\n";
    byte_buf b;

    uint32_t msg_len = 8 + input.length();  //packet header length is 8
    msg_len = byte_order::hton32(msg_len);
    uint32_t proto_type = byte_order::hton32(0);
    b.write_to_byte_buf((char *) &msg_len, sizeof(msg_len));
    b.write_to_byte_buf((char *) &proto_type, sizeof(proto_type));

    b.write_to_byte_buf(input.c_str(),input.length());

    string b_str = b.to_string();

    matrix_compress::compress(b, true);

    string output = b.to_string();
    cout << "input size:" << input.size() << " output size:"
         << output.size() << endl;
    cout << "output: " << output << endl;

    byte_buf b2;
    matrix_compress::uncompress(b,b2);

    if ( b_str   == b2.to_string()) {
        cout << "Equal" << endl;
    } else {
        cout << "ERROR: not equal" << endl;
        cout << b_str << endl;
        cout << b2.to_string() << endl;
    }
}

