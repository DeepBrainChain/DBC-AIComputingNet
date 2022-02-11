#ifndef DBC_CONF_MANAGER_H
#define DBC_CONF_MANAGER_H

#include "util/utils.h"
#include "network/compress/matrix_capacity.h"

//using namespace boost::program_options;
namespace bpo = boost::program_options;

struct peer_seeds {
    const char * seed;
    uint16_t port;
};

struct ImageServer {
    std::string ip;
    std::string port;
    std::string username;
    std::string passwd;
    std::string image_dir;
    std::string id;

    std::string to_string();
    void from_string(const std::string& str);
};

class conf_manager : public Singleton<conf_manager>
{
public:
    conf_manager();

    virtual ~conf_manager() = default;

    ERR_CODE init();

    int32_t get_log_level() const { return m_log_level; }

    std::string get_net_type() const { return m_net_type; }

    int32_t get_net_flag() const { return m_net_flag; }

    std::string get_net_listen_ip() const { return m_net_listen_ip; }

    int32_t get_net_listen_port() const { return m_net_listen_port; }

    int32_t get_max_connect_count() const { return m_max_connect_count; }

    std::string get_http_ip() const { return m_http_ip; }

    int32_t get_http_port() const { return m_http_port; }

    std::vector<std::string> get_dbc_chain_domain() const { return m_dbc_chain_domain; }

    const std::vector<std::string> &get_ip_seeds() { return m_internal_ip_seeds; }

    const std::vector<std::string> &get_dns_seeds() { return m_internal_dns_seeds; }

    const std::vector<std::string> & get_peers() { return m_peers; }

    std::string get_node_id() const { return m_node_id; }

    std::string get_node_private_key() const { return m_node_private_key; }

    std::string get_pub_key() const { return m_pub_key; }

    std::string get_priv_key() const { return m_priv_key; }

    const std::vector<ImageServer>& get_image_servers() const { return m_image_servers; }

    int32_t get_timer_service_broadcast_in_second() {
        return 30;
    }

    int32_t get_timer_service_list_expired_in_second() {
        return 300;
    }

    int32_t  get_timer_dbc_request_in_millisecond() {
        return 20 * 1000;
    }

    int32_t get_timer_ai_training_task_schedule_in_second() {
        return 15;
    }

    int32_t get_timer_log_refresh_in_second() {
        return 10;
    }

    int32_t get_max_recv_speed() {
        return 0;
    }

    int32_t get_update_idle_task_cycle() {
        return  24 * 60;
    }

    const dbc::network::matrix_capacity & get_proto_capacity() {
        return m_proto_capacity;
    }

    int16_t get_prune_container_stop_interval() {
        return 240;
    }

    int16_t get_prune_docker_root_use_ratio() {
        return 5;
    }

    int16_t get_prune_task_stop_interval() {
        return 2400;
    }

protected:
    int32_t parse_local_conf();

    int32_t parse_node_dat();

    ERR_CODE create_new_nodeid();

    int32_t serialize_node_info(const util::machine_node_info &info);

    bool check_node_id();

    void create_crypto_keypair();

private:
    int32_t m_log_level = 2;

    std::string m_net_type;
    int32_t m_net_flag;
    std::string m_net_listen_ip;
    int32_t m_net_listen_port = 10001;
    int32_t m_max_connect_count = 0;

    std::string m_http_ip;
    int32_t m_http_port = 20001;

    std::vector<std::string> m_dbc_chain_domain;

    std::vector<std::string> m_internal_ip_seeds;
    std::vector<std::string> m_internal_dns_seeds;
    std::vector<std::string> m_peers;

    std::string m_node_id;
    std::string m_node_private_key;

    std::string m_pub_key;
    std::string m_priv_key;

    std::string m_version = "0.3.7.3";

    dbc::network::matrix_capacity m_proto_capacity;

    std::vector<ImageServer> m_image_servers;
};

#endif
