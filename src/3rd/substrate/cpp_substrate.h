#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

struct substrate_AccountData {
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

struct substrate_MultisigAccountID {
  const char *account_id;
};

extern "C" {

substrate_AccountData *create_account();

void free_account(substrate_AccountData *ptr);

substrate_SignData *priv_sign(const char *c_priv_key, const char *c_message);

void free_signdata(substrate_SignData *ptr);

bool pub_verify(const char *c_signature, const char *c_message, const char *c_pub_key);

substrate_AccountData *inspect_key(const char *c_key);

substrate_MultisigAccountID *create_multisig_account(const char *accounts, uint16_t threshold);

void free_multisig_account(substrate_MultisigAccountID *ptr);

} // extern "C"
