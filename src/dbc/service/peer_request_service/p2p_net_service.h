#pragma once

#include <boost/asio.hpp>
#include "util/utils.h"
#include "service_module/service_module.h"
#include "network/handler_create_functor.h"
#include "peer_node.h"
#include "peer_candidate.h"
#include "util/crypto/random.h"
#include <leveldb/db.h>

using namespace boost::asio::ip;

#define MIN_PEER_CANDIDATES_COUNT                8
#define MAX_SEND_PEER_NODES_COUNT                16
#define MIN_NORMAL_AVAILABLE_NODE_COUNT          2
#define DISCONNECT_NODE_PER_MINUTES              5

class p2p_net_service : public service_module, public Singleton<p2p_net_service> {
    using peer_list_type = typename std::list<std::shared_ptr<peer_node>>;
    using peer_map_type = typename std::unordered_map<std::string, std::shared_ptr<peer_node>>;

public:
    p2p_net_service() = default;

    ~p2p_net_service() override;

    int32_t init() override;

    std::string get_host_ip() const { return m_listen_ip; }

    uint16_t get_net_listen_port() const { return m_listen_port; }

protected:
    void init_timer() override;

    void init_invoker() override;

    void init_subscription() override;

    int32_t init_conf();

    int32_t init_db();

    int32_t load_peer_candidates_from_db();

    int32_t init_acceptor();

    int32_t init_connector();

    // peer candidate
    bool is_peer_candidate_exist(tcp::endpoint &ep);

    bool add_peer_candidate(tcp::endpoint &ep, net_state ns, peer_node_type ntype, const std::string& node_id = "");

    std::shared_ptr<peer_candidate> get_peer_candidate(const tcp::endpoint &ep);

    void remove_peer_candidate(const tcp::endpoint &ep);

    bool update_peer_candidate_state(tcp::endpoint &ep, net_state ns);

    int32_t get_available_peer_candidates(uint32_t count,
                                          std::vector<std::shared_ptr<peer_candidate>> &available_candidates);

    uint32_t get_available_peer_candidates_count_by_node_type(peer_node_type node_type = NORMAL_NODE);

    int32_t save_peer_candidates();

    int32_t clear_peer_candidates_db();

    uint32_t get_maybe_available_peer_candidates_count();

    // peer node
    void get_all_peer_nodes(peer_list_type &nodes);

    std::shared_ptr<peer_node> get_peer_node(const std::string &id);

    bool add_peer_node(const std::shared_ptr<dbc::network::message> &msg);

    void remove_peer_node(const std::string &id);

    bool exist_peer_node(tcp::endpoint ep);

    bool exist_peer_node(std::string &nid);

    uint32_t get_peer_nodes_count_by_socket_type(dbc::network::socket_type type);


    void on_client_tcp_connect_notification(const std::shared_ptr<dbc::network::message> &msg);

    void on_tcp_channel_error(const std::shared_ptr<dbc::network::message> &msg);


    void on_timer_check_peer_candidates(const std::shared_ptr<core_timer>& timer);

    void on_timer_dyanmic_adjust_network(const std::shared_ptr<core_timer>& timer);

    void on_timer_peer_info_exchange(const std::shared_ptr<core_timer>& timer);

    void on_timer_peer_candidate_dump(const std::shared_ptr<core_timer>& timer);


    void on_ver_req(const std::shared_ptr<dbc::network::message> &msg);

    void on_ver_resp(const std::shared_ptr<dbc::network::message> &msg);


    void on_cmd_get_peer_nodes_req(const std::shared_ptr<dbc::network::message> &msg);

    void on_get_peer_nodes_req(const std::shared_ptr<dbc::network::message> &msg);

    void on_get_peer_nodes_resp(const std::shared_ptr<dbc::network::message> &msg);

    int32_t send_get_peer_nodes();

    int32_t broadcast_peer_nodes(std::shared_ptr<peer_node> node);

    uint32_t get_rand32() { return m_rand_ctx.rand32(); }

    uint32_t start_connect(const tcp::endpoint tcp_ep);

    std::shared_ptr<peer_node> get_dynamic_disconnect_peer_node();

    std::shared_ptr<peer_candidate> get_dynamic_connect_peer_candidate();

    void add_dns_seeds();

    void add_ip_seeds();

    void advertise_local(tcp::endpoint tcp_ep, dbc::network::socket_id sid);

protected:
    uint256 m_rand_seed;
    FastRandomContext m_rand_ctx;

    std::string m_listen_ip;
    uint16_t m_listen_port = 10001;

    std::list<std::string> m_dns_seeds;
    std::list<std::string> m_ip_seeds;

    std::shared_ptr<leveldb::DB> m_peers_candidates_db;
    std::list<std::shared_ptr<peer_candidate>> m_peer_candidates;
    std::unordered_map<std::string, std::shared_ptr<peer_node>> m_peer_nodes_map;  // <node_id, ...>

    uint32_t m_timer_check_peer_candidates = INVALID_TIMER_ID;
    uint32_t m_timer_dyanmic_adjust_network = INVALID_TIMER_ID;
    uint32_t m_timer_peer_info_exchange = INVALID_TIMER_ID;
    uint32_t m_timer_dump_peer_candidates = INVALID_TIMER_ID;
};
