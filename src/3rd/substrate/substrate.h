#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

struct substrate_WalletKepai {
    const char *secret_phrase;
    const char *secret_seed;
    const char *public_key;
    const char *public_key_ss58;
    const char *account_id;
    const char *address_ss58;
};

struct substrate_SignData {
    const char *signature;
};

extern "C" {

substrate_WalletKepai *create_keypair();

void free_keypair(substrate_WalletKepai *ptr);

substrate_SignData *secret_sign(const char *c_seed, const char *c_message);

void free_signdata(substrate_SignData *ptr);

bool public_verify(const char *c_signature, const char *c_message, const char *c_pub_key);

} // extern "C"
