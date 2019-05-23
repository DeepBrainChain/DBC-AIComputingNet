//
// Created by Jimmy on 2019/3/29.
//

#pragma once


#include <string>
#include "core/crypto/key.h"
#include "secp256k1_ecdh.h"

namespace matrix
{
    namespace service_core
    {
        class ai_ecdh_cipher
        {
        public:
            std::string m_data;

            // the pub key with which receiver caculate ecdh share secret.
            std::string m_pub;
        };

        class ai_ecdh_secret
        {
        public:
            ai_ecdh_secret();
            unsigned char m_ecdh_share_secret[32];  //share secret
            CKey m_key;             // local secret
        };

        class ai_ecdh_crypter
        {
        public:
            ai_ecdh_crypter(secp256k1_context *ctx);

            bool decrypt(ai_ecdh_cipher &cipher, CKey& key, std::string &plain_str);

            bool encrypt(CPubKey &cpk_remote, std::string &plain_str, ai_ecdh_cipher &cipher);

        private:
            secp256k1_context *m_ctx;
        };

        class ai_crypto_util
        {
        public:

            static int32_t extra_sign_info(std::string &message, std::map <std::string, std::string> &exten_info);

            static std::string
            derive_nodeid_bysign(std::string &message, std::map <std::string, std::string> &exten_info);

            static bool
            derive_pub_key_bysign(std::string &message, std::map <std::string, std::string> &exten_info, CPubKey &pub);

            static bool
            verify_sign(std::string &message, std::map <std::string, std::string> &exten_info, std::string origin_node);

            static bool get_local_node_private_key(CKey &key);
        };
    }
}


