#include <iostream>
#include <iomanip>
#include "substrate.h"

std::string to_hex(std::string const & input) {
    std::string output;
    std::string hex = "0123456789ABCDEF";

    for (size_t i = 0; i < input.size(); i++) {
        output += hex[(input[i] & 0xF0) >> 4];
        output += hex[input[i] & 0x0F];
    }

    return output;
}

int main() {
    // keypair
    substrate_WalletKepai *data = create_keypair();
    std::cout << std::setw(20) << "secret_phrase: " << data->secret_phrase << std::endl
              << std::setw(20) << "secret_seed: " << data->secret_seed << std::endl
              << std::setw(20) << "public_key: " << data->public_key << std::endl
              << std::setw(20) << "address_ss58: " << data->address_ss58 << std::endl;
    //free_keypair(data);

    // sign
    std::string msg = "hello world!";
    std::cout << std::setw(20) << "message: " << msg << std::endl;
    std::string message = to_hex(msg);
    std::string secret_seed = data->secret_seed;
    substrate_SignData *signdata = secret_sign(secret_seed.c_str(), message.c_str());
    std::cout << std::setw(20) << "signature: " << signdata->signature << std::endl;
    //free_signdata(signdata);

    bool succ = public_verify(signdata->signature, message.c_str(), data->public_key);
    std::cout << std::setw(20) << "verify: " <<  succ << std::endl;



    return 0;
}
