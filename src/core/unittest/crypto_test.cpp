
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


#include "core/crypto/ctaes/ctaes.h"

static void from_hex(unsigned char* data, int len, const char* hex) {
    int p;
    for (p = 0; p < len; p++) {
        int v = 0;
        int n;
        for (n = 0; n < 2; n++) {
            assert((*hex >= '0' && *hex <= '9') || (*hex >= 'a' && *hex <= 'f'));
            if (*hex >= '0' && *hex <= '9') {
                v |= (*hex - '0') << (4 * (1 - n));
            } else {
                v |= (*hex - 'a' + 10) << (4 * (1 - n));
            }
            hex++;
        }
        *(data++) = v;
    }
    assert(*hex == 0);
}

typedef struct {
    int keysize;
    const char* key;
    const char* plain;
    const char* cipher;
} ctaes_test;


static const ctaes_test ctaes_tests[] = {
        /* AES test vectors from FIPS 197. */
        {128, "000102030405060708090a0b0c0d0e0f", "00112233445566778899aabbccddeeff", "69c4e0d86a7b0430d8cdb78070b4c55a"},
        {192, "000102030405060708090a0b0c0d0e0f1011121314151617", "00112233445566778899aabbccddeeff", "dda97ca4864cdfe06eaf70a0ec0d7191"},
        {256, "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "00112233445566778899aabbccddeeff", "8ea2b7ca516745bfeafc49904b496089"},

        /* AES-ECB test vectors from NIST sp800-38a. */
        {128, "2b7e151628aed2a6abf7158809cf4f3c", "6bc1bee22e409f96e93d7e117393172a", "3ad77bb40d7a3660a89ecaf32466ef97"},
        {128, "2b7e151628aed2a6abf7158809cf4f3c", "ae2d8a571e03ac9c9eb76fac45af8e51", "f5d3d58503b9699de785895a96fdbaaf"},
        {128, "2b7e151628aed2a6abf7158809cf4f3c", "30c81c46a35ce411e5fbc1191a0a52ef", "43b1cd7f598ece23881b00e3ed030688"},
        {128, "2b7e151628aed2a6abf7158809cf4f3c", "f69f2445df4f9b17ad2b417be66c3710", "7b0c785e27e8ad3f8223207104725dd4"},
        {192, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "6bc1bee22e409f96e93d7e117393172a", "bd334f1d6e45f25ff712a214571fa5cc"},
        {192, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "ae2d8a571e03ac9c9eb76fac45af8e51", "974104846d0ad3ad7734ecb3ecee4eef"},
        {192, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "30c81c46a35ce411e5fbc1191a0a52ef", "ef7afd2270e2e60adce0ba2face6444e"},
        {192, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "f69f2445df4f9b17ad2b417be66c3710", "9a4b41ba738d6c72fb16691603c18e0e"},
        {256, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "6bc1bee22e409f96e93d7e117393172a", "f3eed1bdb5d2a03c064b5a7e3db181f8"},
        {256, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "ae2d8a571e03ac9c9eb76fac45af8e51", "591ccb10d410ed26dc5ba74a31362870"},
        {256, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "30c81c46a35ce411e5fbc1191a0a52ef", "b6ed21b99ca6f4f9f153e7b1beafed1d"},
        {256, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "f69f2445df4f9b17ad2b417be66c3710", "23304b7a39f9f3ff067d8d8f9e24ecc7"}
};

BOOST_AUTO_TEST_CASE(aes256_test) {
    unsigned char key[32], plain[16], cipher[16], ciphered[16], deciphered[16];
    const ctaes_test* test = &ctaes_tests[2];
//    assert(test->keysize == 128 || test->keysize == 192 || test->keysize == 256);
    from_hex(plain, 16, test->plain);
    from_hex(cipher, 16, test->cipher);

    AES256_ctx ctx;
    from_hex(key, 32, test->key);
    AES256_init(&ctx, key);
    AES256_encrypt(&ctx, 1, ciphered, plain);
    AES256_decrypt(&ctx, 1, deciphered, cipher);


    if (memcmp(cipher, ciphered, 16)) {
        fprintf(stderr, "E(key=\"%s\", plain=\"%s\") != \"%s\"\n", test->key, test->plain, test->cipher);
        BOOST_TEST(false);
    }
    if (memcmp(plain, deciphered, 16)) {
        fprintf(stderr, "D(key=\"%s\", cipher=\"%s\") != \"%s\"\n", test->key, test->cipher, test->plain);
        BOOST_TEST(false);
    }

    BOOST_TEST(true);
}


#include "crypto/aes.h"

BOOST_AUTO_TEST_CASE(cbc_aes256_test) {

    bool padin = true;
    unsigned char key[32] = {0};
    unsigned char iv[16] = {0};
    key[31] = 1;
    iv[15] = 1;

    AES256CBCEncrypt en(key, iv, padin);
    AES256CBCDecrypt de(key, iv, padin);

    std::string plain="Love of my life, you've hurt me\n"
                      "You've broken my heart and now you leave me\n"
                      "Love of my life, can't you see?\n"
                      "Bring it back, bring it back\n"
                      "Don't take it away from me, because you don't know\n"
                      "What it means to me\n"
                      "Love of my life, don't leave me\n"
                      "You've stolen my love, you now desert me\n"
                      "Love of my life, can't you see?\n"
                      "Bring it back, bring it back (back)\n"
                      "Don't take it away from me\n"
                      "Because you don't know\n"
                      "What it means to me\n"
                      "Obrigado\n"
                      "You will remember\n"
                      "When this is blown over\n"
                      "Everything's all by the way\n"
                      "When I grow older\n"
                      "I will be there at your side to remind you\n"
                      "How I still love you (I still love you)\n"
                      "I still love you\n"
                      "Oh, hurry back, hurry back\n"
                      "Don't take it away from me\n"
                      "Because you don't know what it means to me\n"
                      "Love of my life\n"
                      "Love of my life\n"
                      "Ooh, eh (alright)";

    unsigned char* cipher = new unsigned char [plain.size() + 64];
    unsigned char* de_result = new unsigned char [plain.size()+1];
    memset(cipher, 0, plain.size() + 64);
    memset(de_result, 0,plain.size()+1);

    const unsigned char * plain_ = (const unsigned char*) plain.c_str();
    int cipher_size = en.Encrypt(plain_, plain.size(), cipher);

    BOOST_TEST(cipher_size <= plain.size()+16);

    int len = de.Decrypt(cipher, cipher_size, de_result);

    BOOST_TEST(plain == std::string((char*)de_result, len) );

    delete[] cipher;
    delete[] de_result;
//    std::cout << (char*)de_result <<std::endl;

}


#include "secp256k1_ecdh.h"

static void counting_illegal_callback_fn(const char *str, void *data)
{
    /* Dummy callback function that just counts. */
}


//#include "scalar.h"
//
//void random_scalar_order(secp256k1_scalar *num) {
//    do {
//        unsigned char b32[32];
//        int overflow = 0;
//        secp256k1_rand256(b32);
//        secp256k1_scalar_set_b32(num, b32, &overflow);
//        if (overflow || secp256k1_scalar_is_zero(num)) {
//            continue;
//        }
//        break;
//    } while(1);
//}

BOOST_AUTO_TEST_CASE(ecdh_test) {
//    secp256k1_context *tctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
//    secp256k1_pubkey point;
//    unsigned char res[32];
//    unsigned char s_one[32] = { 0 };
//    int32_t ecount = 0;
//    s_one[31] = 1;
//
//    secp256k1_context_set_error_callback(tctx, counting_illegal_callback_fn, &ecount);
//    secp256k1_context_set_illegal_callback(tctx, counting_illegal_callback_fn, &ecount);
//    BOOST_TEST(secp256k1_ec_pubkey_create(tctx, &point, s_one) == 1);
//
//    /* Check all NULLs are detected */
//    BOOST_TEST(secp256k1_ecdh(tctx, res, &point, s_one) == 1);
//    BOOST_TEST(ecount == 0);


    secp256k1_context * ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    unsigned char s_one[32] = { 0 };
    unsigned char output_one[32];
    s_one[31] = 1;

    unsigned char s_two[32] = { 0 };
    unsigned char output_two[32];
    s_two[31] = 2;

    secp256k1_pubkey p_one;
    secp256k1_pubkey p_two;

    BOOST_TEST(secp256k1_ec_pubkey_create(ctx, &p_one, s_one) == 1);
    BOOST_TEST(secp256k1_ec_pubkey_create(ctx, &p_two, s_two) == 1);

    BOOST_TEST(secp256k1_ecdh(ctx, output_one, &p_two, s_one) == 1);
    BOOST_TEST(secp256k1_ecdh(ctx, output_two, &p_one, s_two) == 1);

    BOOST_TEST(output_one == output_two);

    secp256k1_context_destroy(ctx);
}