#ifndef DBC_CONF_MANAGER_H
#define DBC_CONF_MANAGER_H

#include "network/compress/matrix_capacity.h"
#include "util/utils.h"

namespace bpo = boost::program_options;

struct peer_seeds {
    const char* seed;
    uint16_t port;
};

class ConfManager : public Singleton<ConfManager> {
public:
    ConfManager();

    virtual ~ConfManager();

    ERRCODE Init();

    std::string GetVersion() const { return m_version; }

    int32_t GetLogLevel() const { return m_log_level; }

    std::string GetNetType() const { return m_net_type; }

    int32_t GetNetFlag() const { return m_net_flag; }

    std::string GetNetListenIp() const { return m_net_listen_ip; }

    int32_t GetNetListenPort() const { return m_net_listen_port; }

    int32_t GetMaxConnectCount() const { return m_max_connect_count; }

    std::string GetHttpListenIp() const { return m_http_listen_ip; }

    int32_t GetHttpListenPort() const { return m_http_listen_port; }

    std::vector<std::string> GetDbcChainDomain() const {
        return m_dbc_chain_domain;
    }

    std::string GetChainAdditionalField() const {
        return m_chain_additional_field;
    }

    bool IsTerminatingRental() const {
        return m_chain_additional_field == "terminatingRental";
    }

    const std::map<std::string, ImageServer*>& GetImageServers() const {
        return m_image_server;
    }

    ImageServer* FindImageServer(const std::string& id) const {
        auto it = m_image_server.find(id);
        if (it != m_image_server.end()) {
            return (*it).second;
        } else {
            return nullptr;
        }
    }

    std::string GetDbcMonitorServer() const { return m_dbc_monitor_server; }

    std::string GetMinerMonitorServer() const { return m_miner_monitor_server; }

    std::string GetMulticastAddress() const { return m_multicast_address; }

    int32_t GetMulticastPort() const { return m_multicast_port; }

    uint32_t GetCheckVmExpirationFrequency() const {
        return m_check_vm_expiration_timer_frequency;
    }

    std::string GetCloudCybercafeServer() const {
        return m_cloud_cybercafe_server;
    }

    std::string GetNetbarPowerManager() const { return m_netbar_power_manager; }

    const std::vector<std::string>& GetPeers() { return m_peers; }

    const std::vector<std::string>& GetInternalIpSeeds() {
        return m_internal_ip_seeds;
    }

    const std::vector<std::string>& GetInternalDnsSeeds() {
        return m_internal_dns_seeds;
    }

    std::string GetNodeId() const { return m_node_id; }

    std::string GetNodePrivateKey(const std::string& node_id = "") const;

    std::string GetPubKey() const { return m_pub_key; }

    std::string GetPrivKey() const { return m_priv_key; }

    const network::matrix_capacity& get_proto_capacity() {
        return m_proto_capacity;
    }

protected:
    ERRCODE ParseConf();

    ERRCODE ParseDat();

    ERRCODE CreateNewNodeId();

    ERRCODE SerializeNodeInfo(const util::machine_node_info& info);

    bool CheckNodeId();

    void CreateCryptoKeypair();

private:
    std::string m_version = "0.3.7.3";
    int32_t m_log_level = 2;
    std::string m_net_type = "mainnet";
    int32_t m_net_flag = 0xF1E1B0F9;
    std::string m_net_listen_ip = "0.0.0.0";
    int32_t m_net_listen_port = 5001;
    int32_t m_max_connect_count = 1024;
    std::string m_http_listen_ip = "127.0.0.1";
    int32_t m_http_listen_port = 5050;
    std::vector<std::string> m_dbc_chain_domain;
    std::string m_chain_additional_field;
    std::map<std::string, ImageServer*> m_image_server;
    std::string m_dbc_monitor_server;
    std::string m_miner_monitor_server;
    std::string m_multicast_address;
    int32_t m_multicast_port;
    uint32_t m_check_vm_expiration_timer_frequency;
    std::string m_cloud_cybercafe_server;
    std::string m_netbar_power_manager;

    std::vector<std::string> m_peers;
    std::vector<std::string> m_internal_ip_seeds;
    std::vector<std::string> m_internal_dns_seeds;

    std::string m_node_id;
    std::string m_node_private_key;
    std::string m_pub_key;
    std::string m_priv_key;

    network::matrix_capacity m_proto_capacity;
};

#endif
