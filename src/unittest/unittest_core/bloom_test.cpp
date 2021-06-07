
#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "bloom.h"
#include <ctime>
#include <stdlib.h>
#include <string>

//using namespace matrix::core;
const int SIZE_CHAR = 32;
const char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string get_str()
{
	  const int SIZE_CHAR = 32;
    const char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		srand((unsigned)time(NULL));
    //char ch[SIZE_CHAR + 1] = { 0 };
    std::string test_str="";
    for (int i = 0; i < SIZE_CHAR; ++i)
    {
        int x = rand() / (RAND_MAX / (sizeof(CCH) - 1));
        test_str += CCH[x];
    }
    return 	test_str;
} 
BOOST_AUTO_TEST_CASE(test_bloomfilter) {

    CRollingBloomFilter bloom_filter(120000,0.00001);
    std::string test_str = get_str();
    if (bloom_filter.contains(test_str))
    {
    		
    }
    else
    {
    		bloom_filter.insert(test_str);
   	}
    BOOST_TEST(true);
}

