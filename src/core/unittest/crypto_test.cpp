
#include "core/crypto/key.h"
#include "core/crypto/crypto_service.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include "core/crypto/sha512.h"
#include "core/crypto/random.h"

// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


using namespace boost::unit_test;
using namespace std;



void print(CKey& k) {
    for (auto b = k.begin(); b < k.end(); b++){
        std::cout<<" 0x"<<std::hex<<(int)*b;
    }
    std::cout<<std::endl;
}

void fprint(ofstream& f, CKey& k) {

    for (auto b = k.begin(); b < k.end(); b++){
        f<<std::hex<<std::setfill('0') << std::setw(2)<<(int)*b;
    }

    f << "\n";

}


BOOST_AUTO_TEST_CASE(make_seceret_test) {
    crypto_service s;
    bpo::variables_map c;
    s.init(c);

//    std::ofstream myfile ("key.txt");
//    if (myfile.is_open()) {
//
//
        CKey k;
//        for (int i = 0; i < 1; i++) {
            k.MakeNewKey(false);

//
//            fprint(myfile, k);
//        }
//        myfile.close();
//    }


    print(k);
}

#include "core/crypto/base58.h"
BOOST_AUTO_TEST_CASE(base58_encode_test) {
    unsigned char a[]={1,3};
    auto s = EncodeBase58(a,a+2);

    std::cout<<s<<std::endl;

}

#include <cstdio>
void print_array(unsigned char a[], int len) {
    for (int i=0; i<len; i++){
        printf(" 0x%02hhX",a[i]);
    }
    std::cout<<std::endl;
}

BOOST_AUTO_TEST_CASE(sha512_test) {
    CSHA512 h;
    const unsigned char buf[] = "";
    unsigned char out[64] = {0};
    h.Write((const unsigned char*)buf, 0).Finalize((unsigned char*)out);
//    print_array(out, 64);

    BOOST_TEST(0xcf == out[0]);
    BOOST_TEST(0x3e == out[63]);
}