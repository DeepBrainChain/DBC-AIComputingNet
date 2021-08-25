#include "util/utils.h"
#include "service/message/matrix_types.h"
#include "network/protocol/thrift_binary.h"
#include "tweetnacl/tools.h"
#include "tweetnacl/randombytes.h"
#include "tweetnacl/tweetnacl.h"
#include "util/base64.h"
#include <functional>
#include <chrono>
#include "log/log.h"
#include <csignal>
#include "server/server.h"

high_resolution_clock::time_point server_start_time;

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cout << "usage: " << std::endl
                  << "  sign_tool [wallet] [wallet_private_key] [session_id](可选)" << std::endl;
        return 0;
    }

    std::string wallet = argv[1];
    std::string wallet_private_key = argv[2];
    std::string session_id;
    if (argc > 3) {
        session_id = argv[3];
    }

    std::string nonce = util::create_nonce();
    std::string nonce_sign = util::sign(nonce, wallet_private_key);
    bool succ = util::verify_sign(nonce_sign, nonce, wallet);
    if (!succ) {
        std::cout << "input wallet & wallet_private_key is error!" << std::endl;
        return 0;
    }
    std::cout << std::setw(12) << "wallet: " << wallet << std::endl
              << std::setw(12) << "nonce: " << nonce << std::endl
              << std::setw(12) << "nonce_sign: " << nonce_sign << std::endl;

    if (!session_id.empty()) {
        std::string signature = util::sign(session_id, wallet_private_key);
        std::cout << std::setw(18) << "session_id: " << session_id << std::endl
                  << std::setw(18) << "session_id_sign: " << signature << std::endl;
    }

    return 0;
}
