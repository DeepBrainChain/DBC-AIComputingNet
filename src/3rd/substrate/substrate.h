#ifndef DBC_SUBSTRATE_H
#define DBC_SUBSTRATE_H

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

struct substrate_WalletKepair {
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

substrate_WalletKepair *create_keypair();

void free_keypair(substrate_WalletKepair *ptr);

substrate_SignData *secret_sign(const char *c_seed, const char *c_message);

void free_signdata(substrate_SignData *ptr);

bool public_verify(const char *c_signature, const char *c_message, const char *c_pub_key);

substrate_WalletKepair *inspect_key(const char *c_seed);

} // extern "C"

#endif
