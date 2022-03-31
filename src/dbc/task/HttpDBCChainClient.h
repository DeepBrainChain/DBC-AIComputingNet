#ifndef DBC_HTTP_DBC_CHAIN_CLIENT_H
#define DBC_HTTP_DBC_CHAIN_CLIENT_H

#include "util/utils.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "util/httplib.h"
#include "network/utils/net_address.h"

class HttpDBCChainClient : public Singleton<HttpDBCChainClient> {
public:
    HttpDBCChainClient();

    virtual ~HttpDBCChainClient();

    void init(const std::vector<std::string>& dbc_chain_addrs);

    int64_t request_cur_block();

    MACHINE_STATUS request_machine_status(const std::string& node_id);

    bool in_verify_time(const std::string& node_id, const std::string& wallet);

    int64_t request_rent_end(const std::string& node_id, const std::string &wallet);

    void request_cur_renter(const std::string& node_id, std::string& renter, int64_t& rent_end);

private:
    std::map<int32_t, network::net_address> m_addrs;
};

#endif
