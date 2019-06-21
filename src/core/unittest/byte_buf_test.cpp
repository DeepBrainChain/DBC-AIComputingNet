#include <boost/test/unit_test.hpp>


//using namespace boost::unit_test;
using namespace std;

#include "byte_buf.h"

using namespace matrix::core;

namespace utf = boost::unit_test;

//BOOST_AUTO_TEST_SUITE(suite_byte_buf, * utf::enabled())


    BOOST_AUTO_TEST_CASE(test_write) {

       byte_buf b(1024);
       char s[]="123456";
       b.write_to_byte_buf(s, sizeof(s));
       BOOST_TEST(b.get_valid_read_len(), sizeof(s));
    }

    BOOST_AUTO_TEST_CASE(test_move) {

        byte_buf b(1024);

        char s[]="123456";
        b.write_to_byte_buf(s, sizeof(s));

        BOOST_TEST(b.get_valid_read_len(), sizeof(s));

        uint8_t out[1024];
        memset(out, 0xff, 1024);
        b.read_from_byte_buf((char*)out,2);

        b.move_buf();

    }

    BOOST_AUTO_TEST_CASE(test_write_over_capacity_wo_auto_alloc) {

        byte_buf b(4,false);
        char s[]="123456";

        BOOST_CHECK_THROW(b.write_to_byte_buf(s, sizeof(s)), std::length_error);
    }

    BOOST_AUTO_TEST_CASE(test_write_auto_alloc) {
        byte_buf b(4);
        char s[]="123456";
        b.write_to_byte_buf(s, sizeof(s));
        BOOST_TEST(b.get_valid_read_len(), sizeof(s));
    }


    BOOST_AUTO_TEST_CASE(test_write_huge_data) {

        byte_buf b(1024);
        uint32_t l = MAX_BYTE_BUF_LEN-1;
        char *s = new char[l];

        b.write_to_byte_buf(s, l);
        BOOST_TEST(b.get_valid_read_len(), l);
        delete []s;
    }

    //BOOST_AUTO_TEST_CASE(test_write_zero_byte) {
    //
    //    byte_buf b(1024);
    //    char s[]="123456";
    //
    //    b.write_to_byte_buf(s, 0);
    //    BOOST_TEST(b.get_valid_read_len(), 0);
    //}

    //BOOST_AUTO_TEST_CASE(test_write_nil_data) {
    //
    //    byte_buf b(1024);
    //
    //    b.write_to_byte_buf(nullptr, 0);
    //    BOOST_TEST(b.get_valid_read_len(), 0);
    //}

    BOOST_AUTO_TEST_CASE(test_write_beyond_threshold) {

        byte_buf b(1024);
        uint32_t l = MAX_BYTE_BUF_LEN + 1;
        char *s = new char[l];

        BOOST_CHECK_THROW(b.write_to_byte_buf(s, l), std::length_error);
        delete []s;
    }


    BOOST_AUTO_TEST_CASE(test_read_all) {

        byte_buf b(1024);
        char s[]="123456";

        b.write_to_byte_buf(s, sizeof(s));

        char output[1024];
        b.read_from_byte_buf(output, sizeof(s));

        BOOST_TEST_MESSAGE(sizeof(s));
    }

BOOST_AUTO_TEST_CASE(test_buf_to_string)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(1024);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));
  std::string buf_string = buf->to_string();

  BOOST_TEST(buf_string == "31 32 33 34 35 36 00 ");
}

BOOST_AUTO_TEST_CASE(test_get_valid_write_len)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(7);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));

  uint32_t len = buf->get_valid_write_len();
  BOOST_TEST(len == 0);
}

BOOST_AUTO_TEST_CASE(test_get_valid_write_len_1)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(2);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));

  uint32_t len = buf->get_valid_write_len();
  BOOST_TEST(len != 0);  // adjust length 2*4,valid_write_len = 8-7
}

BOOST_AUTO_TEST_CASE(test_get_valid_write_len_2)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(7);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));

  uint32_t len = buf->get_valid_write_len();
  BOOST_TEST(len == 0);
}

BOOST_AUTO_TEST_CASE(test_move_write_ptr)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(8);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));

  int32_t ret = buf->move_write_ptr(1);
  BOOST_TEST(ret == E_SUCCESS);

  BOOST_CHECK_THROW(buf->move_write_ptr(1), std::length_error);
}

BOOST_AUTO_TEST_CASE(test_move_read_ptr)
{
  std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>(8);
  char s[]="123456";

  buf->write_to_byte_buf(s, sizeof(s));

  int32_t len = buf->read_from_byte_buf(s, sizeof(s) - 1 );
  BOOST_TEST(len == sizeof(s) - 1 );

  int32_t ret = buf->move_read_ptr(1);
  BOOST_TEST(ret == E_SUCCESS);

  BOOST_CHECK_THROW(buf->move_read_ptr(1), std::length_error);
}
