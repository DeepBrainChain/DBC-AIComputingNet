//
// Created by Jimmy on 2019/3/29.
//


#include "ai_crypter.h"

#include "log/log.h"
#include "crypto/aes.h"
#include "lax_der_privatekey_parsing.h"

#include "server.h"
#include "conf_manager.h"

#include <boost/format.hpp>

namespace matrix { namespace service_core {


int32_t ai_crypto_util::extra_sign_info(std::string &message, std::map <std::string, std::string> &exten_info)
{
    time_t cur = std::time(nullptr);
    std::string sign_message = boost::str(boost::format("%s%d") % message % cur);
    std::string sign = id_generator::sign(sign_message, CONF_MANAGER->get_node_private_key());
    if (sign.empty())
    {
        return E_DEFAULT;
    }

    exten_info["sign"] = sign;
    exten_info["sign_algo"] = ECDSA;
    exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
    return E_SUCCESS;
}

std::string
ai_crypto_util::derive_nodeid_bysign(std::string &message, std::map <std::string, std::string> &exten_info)
{
    if (exten_info["sign"].empty() || exten_info["sign_algo"].empty() || exten_info["sign_at"].empty())
    {
        return "";
    }

    std::string sign_message = boost::str(boost::format("%s%s") % message % exten_info["sign_at"]);
    std::string derive_node_id;

    id_generator::derive_node_id_by_sign(sign_message, exten_info["sign"], derive_node_id);

    return derive_node_id;
}

bool ai_crypto_util::derive_pub_key_bysign(std::string &message, std::map <std::string, std::string> &exten_info,
                                          CPubKey &pub)
{

    if (exten_info["sign"].empty() || exten_info["sign_algo"].empty() || exten_info["sign_at"].empty())
    {
        return false;
    }

    std::string sign_message = boost::str(boost::format("%s%s") % message % exten_info["sign_at"]);

    bool rtn = id_generator::derive_pub_key_by_sign(sign_message, exten_info["sign"], pub);

    return rtn;
}

bool ai_crypto_util::verify_sign(std::string &message, std::map <std::string, std::string> &exten_info,
                                std::string origin_node)
{
    if (use_sign_verify())
    {
        if (origin_node.empty())
        {
            return false;
        }

        return (origin_node == derive_nodeid_bysign(message, exten_info));
    }

    return true;
}

/**
 *  get secret related to local node id
 * @param key
 * @return
 */
bool ai_crypto_util::get_local_node_private_key(CKey &key)
{
    secp256k1_context * ctx = static_cast<secp256k1_context *>(get_context_sign());
    if (nullptr == ctx)
    {
        LOG_ERROR << "fail to get context";
        return false;
    }

    unsigned char privkey_c[32];

    std::string node_private_key = CONF_MANAGER->get_node_private_key();
    std::vector<unsigned char> vch;
    if (!id_generator::decode_private_key(node_private_key, vch))
    {
        LOG_ERROR << "decode_private_key fail";
        return false;
    }

    CPrivKey prikey;
    prikey.insert(prikey.end(), vch.begin(), vch.end());

    if (1 != ec_privkey_import_der(ctx, privkey_c, prikey.data(), prikey.size()))
    {
        LOG_ERROR << "fail to import der";
        return false;
    }

    key.Set(privkey_c, privkey_c + 32, true);

    return true;
}


ai_ecdh_crypter::ai_ecdh_crypter(secp256k1_context *ctx) : m_ctx(ctx)
{
//            m_ctx = static_cast<secp256k1_context *>(get_context_sign());

}


/**
 * encryt plain text with CBCAES 256. The key is calculated with ECDH method.
 *      ecdh (remote node's public key, one time private secret)
 *      need to send out both cipher text and the one time public key.
 * @param pub_str: remote node's public key
 * @param plain_str: plain text
 * @param ai_ecdh_cipher: pub and data
 * @return
 */
bool ai_ecdh_crypter::encrypt(CPubKey &cpk_remote, std::string &plain_str, ai_ecdh_cipher &cipher)
{
    if (!m_ctx)
    {
        LOG_ERROR << "context is null";
        return false;
    }

    // *** get public key of remote node
    secp256k1_pubkey pk_remote;
    if (!cpk_remote.DeSerialize(pk_remote))
    {
        LOG_ERROR << "fail to deserialize public key";
        return false;
    }

    // *** calculate ecdh share secret
    ai_ecdh_secret onetime;
    while (1)
    {
        onetime.m_key.MakeNewKey(true);
        if (1 == secp256k1_ecdh(m_ctx, onetime.m_ecdh_share_secret, &pk_remote, onetime.m_key.begin()))
        {
            break;
        }

        LOG_ERROR << "fail to calculate ecdh share secret";
    }

    // *** cbcaes256 cipher
    bool padin = true;
    unsigned char* key = onetime.m_ecdh_share_secret;
    unsigned char iv[16] = {0};
    iv[15] = 1;

    AES256CBCEncrypt en(key, iv, padin);

    const unsigned char *plain = (const unsigned char *) plain_str.c_str();
    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCKSIZE bytes
    // here, we allocate additional space (64-AES_BLOCKSIZE) to avoid array write out of boundry.

    cipher.m_data.resize(plain_str.size() + 64);

    int cipher_size = en.Encrypt(plain, plain_str.size(), (unsigned char*)cipher.m_data.data());

    assert(cipher_size < plain_str.size() + 64);

    cipher.m_data.resize(cipher_size);

    auto pb = onetime.m_key.GetPubKey();
    cipher.m_pub = std::string((char *) pb.begin(), pb.size());

    return true;
}


/**
 * decrypt cipher with CBCAES 256. The session key is caculated by ECDH.
 *      ecdh (one time pub of sender, node's permanent private key)
 *
 * @param pub_str
 * @param cipher_str
 * @return
 */
bool ai_ecdh_crypter::decrypt(ai_ecdh_cipher &cipher, CKey& key, std::string &plain_str)
{
    if (!m_ctx)
    {
        LOG_ERROR << "context is null";
        return false;
    }

    ai_ecdh_secret onetime;

    // *** extract one time public key from sender
    std::string &pub_str = cipher.m_pub;

    CPubKey pk;
    pk.Set(pub_str.data(), pub_str.data() + pub_str.size());
    secp256k1_pubkey pk_onetime;
    pk.DeSerialize(pk_onetime);


    // *** get local node private key
//    if (!ai_crypto_util::get_local_node_private_key(onetime.m_key))
//    {
//        LOG_ERROR << " fail to get local node's private key";
//        return false;
//    }
    onetime.m_key = key;

    // *** calculate ecdh share secret_onetime
    if (1 != secp256k1_ecdh(m_ctx, onetime.m_ecdh_share_secret, &pk_onetime, onetime.m_key.begin()))
    {
        LOG_ERROR << "fail to calculate ecdh secret";
        return false;
    }

    // *** cbc aes 256 decrypt
    {
        bool padin = true;
        unsigned char *key = onetime.m_ecdh_share_secret;
        unsigned char iv[16] = {0};
        iv[15] = 1;

        AES256CBCDecrypt de(key, iv, padin);

        int cipher_size = cipher.m_data.size();

        const unsigned char *cipher_c = (const unsigned char *) cipher.m_data.data();
        // max ciphertext len for a n bytes of plaintext is
        // n + AES_BLOCKSIZE bytes
        // here, we allocate additional space (64-AES_BLOCKSIZE) to avoid array write out of boundry.

        plain_str.resize(cipher_size + 64);

        int plain_size = de.Decrypt(cipher_c, cipher_size,(unsigned char*) plain_str.data());

        assert(plain_size < cipher_size + 64);

        plain_str.resize(plain_size);
    }

    return true;
}


ai_ecdh_secret::ai_ecdh_secret()
{
    memset(m_ecdh_share_secret, 0, 32);
}

}} //name space ai::dbc
