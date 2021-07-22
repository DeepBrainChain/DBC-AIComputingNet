#ifndef DBC_CRYPT_UTIL_H
#define DBC_CRYPT_UTIL_H

#include <iostream>
#include <string>
#include <iomanip>
#include <map>

namespace util {
    struct machine_node_info
    {
        std::string node_id;
        std::string node_private_key;
    };

    int32_t create_node_info(machine_node_info &info);

    std::string create_session_id();

    std::string create_nonce();

    std::string create_task_id();

    bool check_id(const std::string& id);

    std::string derive_pubkey_by_privkey(const std::string& privkey);

    std::string sign(const std::string& message, const std::string& privkey);

    bool verify_sign(const std::string& signature, const std::string& message, const std::string& pubkey);
}

#endif //DBC_CRYPT_UTIL_H
