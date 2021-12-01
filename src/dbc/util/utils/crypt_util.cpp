#include "crypt_util.h"
#include "cpp_substrate.h"
#include <uuid/uuid.h>
#include "../crypto/base58.h"
#include "common/common.h"

namespace util {
    static std::string to_hex(std::string const& input) {
        std::string output;
        std::string hex = "0123456789ABCDEF";

        for (size_t i = 0; i < input.size(); i++) {
            output += hex[(input[i] & 0xF0) >> 4];
            output += hex[input[i] & 0x0F];
        }

        return output;
    }

    int32_t create_node_info(machine_node_info &info) {
        substrate_AccountData *data = create_account();
        std::string pub_key = data->public_key;
        pub_key = pub_key.substr(2);
        std::string priv_key = data->secret_seed;
        priv_key = priv_key.substr(2);

        info.node_id = pub_key;
        info.node_private_key = priv_key;

        free_account(data);
        return E_SUCCESS;
    }

    std::string create_session_id() {
        uuid_t uid;
        uuid_generate(uid);
        char buf[128] = {0};
        uuid_unparse_lower(uid, buf);
        std::string str(buf);
        std::vector<unsigned char> input;
        for (int i = 0; i < str.size(); i++) {
            input.push_back(str[i]);
        }
        return EncodeBase58Check(input);
    }

    std::string create_nonce() {
        uuid_t uid;
        uuid_generate(uid);
        char buf[128] = {0};
        uuid_unparse_lower(uid, buf);
        std::string str(buf);
        std::vector<unsigned char> input;
        for (int i = 0; i < str.size(); i++) {
            input.push_back(str[i]);
        }
        return EncodeBase58Check(input);
    }

    std::string create_task_id() {
        struct timeval tv;
        gettimeofday(&tv, 0);
        int64_t t = tv.tv_usec;
        uint32_t r1 = rand() % 1000000;
        std::string str = std::to_string(r1) + std::to_string(t);
        /*
        uuid_t uid;
        uuid_generate(uid);
        char buf[128] = {0};
        uuid_unparse_lower(uid, buf);
        std::string str(buf);
        */

        std::vector<unsigned char> input;
        for (int i = 0; i < str.size(); i++) {
            input.push_back(str[i]);
        }
        return EncodeBase58Check(input);
    }

    bool check_id(const std::string& id) {
        std::vector<uint8_t> vch_node;
        return DecodeBase58Check(id, vch_node);
    }

    std::string derive_pubkey_by_privkey(const std::string& privkey) {
        std::string priv_key = privkey;
        if (priv_key.substr(0, 2) != "0x") {
            priv_key = "0x" + priv_key;
        }
        substrate_AccountData *data = inspect_key(priv_key.c_str());

        /*
        std::cout << std::setw(20) << "secret_phrase: " << data->secret_phrase << std::endl
                  << std::setw(20) << "secret_seed: " << data->secret_seed << std::endl
                  << std::setw(20) << "public_key: " << data->public_key << std::endl
                  << std::setw(20) << "address_ss58: " << data->address_ss58 << std::endl;
        */

        std::string pubkey(data->public_key);
        free_account(data);

        return pubkey.substr(2);
    }

    std::string sign(const std::string& message, const std::string& privkey) {
        std::string priv_key = privkey;
        if (priv_key.substr(0, 2) != "0x") {
            priv_key = "0x" + priv_key;
        }
        std::string message_hex = to_hex(message);
        substrate_SignData *signdata = priv_sign(priv_key.c_str(), message_hex.c_str());
        std::string sig = signdata->signature;
        free_signdata(signdata);
        return sig;
    }

    bool verify_sign(const std::string& signature, const std::string& message, const std::string& pubkey) {
        std::string pub_key = pubkey;
        if (pub_key.substr(0, 2) != "0x") {
            pub_key = "0x" + pub_key;
        }
        std::string message_hex = to_hex(message);
        bool succ = pub_verify(signature.c_str(), message_hex.c_str(), pub_key.c_str());
        return succ;
    }


}

/* * * * * * test * * * * *
 * * * * * * * * * * * * *
dbc::machine_node_info info;
dbc::create_node_info(info);
std::cout << info.node_id << std::endl
<< info.node_private_key << std::endl;

std::string session_id = dbc::create_session_id();
std::cout << session_id << std::endl;
std::cout << dbc::check_id(session_id) << std::endl;
std::string nonce = dbc::create_nonce();
std::cout << nonce << std::endl;
std::cout << dbc::check_id(nonce) << std::endl;
std::string task_id = dbc::create_task_id();
std::cout << task_id << std::endl;
std::cout << dbc::check_id(task_id) << std::endl;

dbc::derive_pubkey_by_privkey("0x" + info.node_private_key);

std::string sig = dbc::sign("hello", "0x" + info.node_private_key);
std::cout << sig << std::endl;

bool verfiy = dbc::verify_sign(sig, "hello", "0x" + info.node_id);
std::cout << verfiy << std::endl;
*/