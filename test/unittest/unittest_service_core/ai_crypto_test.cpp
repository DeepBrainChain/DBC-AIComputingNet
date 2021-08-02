#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

#include "common/ai_crypter.h"
#include "random.h"
#include "sha256.h"
#include "key.h"

using namespace matrix::service_core;

BOOST_AUTO_TEST_CASE(test_ai_ecdh_crypter){

    RandAddSeed();                          // Seed OpenSSL PRNG with performance counter

    //elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    RandomInit();
    ECC_Start();

    auto handler = ECCVerifyHandle();

    ECC_InitSanityCheck();

    Random_SanityCheck();


    //
    ai_ecdh_crypter crypter(static_cast<secp256k1_context *>(get_context_sign()));

    std::string plain_str= "Love of my life, you've hurt me\n"
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


    CKey key;
    key.MakeNewKey(true);
    auto cpk_remote= key.GetPubKey();


    ai_ecdh_cipher cipher;
    crypter.encrypt(cpk_remote, plain_str, cipher);

    std::string plain2_str;
    crypter.decrypt(cipher, key, plain2_str);

    BOOST_TEST(plain_str == plain2_str);


}
