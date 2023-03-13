#ifndef DBC_HTTP_DBC_CHAIN_CLIENT_H
#define DBC_HTTP_DBC_CHAIN_CLIENT_H

#include "util/utils.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "network/utils/net_address.h"
#include "util/httplib.h"

namespace dbc {
class db_rent_order;
}

struct CommitteeUploadInfo {
    std::string machine_id;
    std::string gpu_type;
    uint32_t gpu_num;
    uint32_t cuda_core;
    uint64_t gpu_mem;
    uint64_t calc_point;  //算力值
    uint64_t sys_disk;
    uint64_t data_disk;
    std::string cpu_type;
    uint32_t cpu_core_num;
    uint64_t cpu_rate;
    uint64_t mem_num;
    // std::vector<std::string> rand_str
    bool is_support;  // 是否支持上链
};

struct chainScore {
    int64_t block;
    int32_t time;

    bool operator<(const chainScore& other) const;
};

class HttpDBCChainClient : public Singleton<HttpDBCChainClient> {
public:
    HttpDBCChainClient();

    virtual ~HttpDBCChainClient();

    void init(const std::vector<std::string>& dbc_chain_addrs);

    void update_chain_order(const std::vector<std::string>& dbc_chain_addrs);

    int64_t request_cur_block();

    MACHINE_STATUS request_machine_status(const std::string& node_id);

    bool in_verify_time(const std::string& node_id, const std::string& wallet);

    // int64_t request_rent_end(const std::string& node_id, const std::string
    // &wallet);

    void request_cur_renter(const std::string& node_id, std::string& renter,
                            int64_t& rent_end);

    bool getCommitteeUploadInfo(const std::string& node_id,
                                CommitteeUploadInfo& info);

    std::shared_ptr<dbc::db_rent_order> getRentOrder(
        const std::string& node_id, const std::string& rent_order);

    // 旧的整机租用方式不支持此接口，返回false。以此区别是否单卡租用
    bool getRentOrderList(const std::string& node_id,
                          std::set<std::string>& orders);

    bool isMachineRenter(const std::string& node_id, const std::string& wallet);

protected:
    chainScore getChainScore(const network::net_address& addr, int delta) const;

private:
    mutable RwMutex m_mtx;
    std::map<chainScore, network::net_address> m_addrs;
};

#endif
