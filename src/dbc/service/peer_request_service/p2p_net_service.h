#ifndef P2P_NET_SERVICE_H
#define P2P_NET_SERVICE_H

#include "util/utils.h"
#include <boost/asio.hpp>
#include "service_module/service_module.h"
#include "network/channel/handler_create_functor.h"
#include "peer_node.h"
#include "peer_candidate.h"
#include "util/crypto/random.h"
#include "data/db/peer_candidate_db.h"

using namespace boost::asio::ip;

class p2p_net_service : public service_module, public Singleton<p2p_net_service> {
public:
    p2p_net_service() = default;

    ~p2p_net_service() override = default;

    ERRCODE init() override;
    
    void exit() override;

    std::string get_host_ip() const { return m_listen_ip; }

    uint16_t get_net_listen_port() const { return m_listen_port; }

    const std::list<std::shared_ptr<peer_candidate>>& get_peer_candidates() const {
        return m_peer_candidates;
    }

    const std::unordered_map<std::string, std::shared_ptr<peer_node>>& get_peer_nodes_map() const {
        return m_peer_nodes_map;
    }

protected:
    void init_timer() override;

    void init_invoker() override;

    ERRCODE init_conf();

    ERRCODE init_db();

    void load_peer_candidates_from_db();

    ERRCODE start_acceptor();

    ERRCODE start_connector();
    

    bool is_peer_candidate_exist(tcp::endpoint &ep);

    bool add_peer_candidate(tcp::endpoint &ep, net_state ns, peer_node_type ntype, const std::string& node_id = "");

    std::shared_ptr<peer_candidate> get_peer_candidate(const tcp::endpoint &ep);

    void remove_peer_candidate(const tcp::endpoint &ep);

    bool update_peer_candidate_state(tcp::endpoint &ep, net_state state);

    int32_t get_maybe_available_peer_candidates_count();


    bool exist_peer_node(tcp::endpoint ep);

    bool exist_peer_node(std::string& node_id);

    std::shared_ptr<peer_node> get_peer_node(const std::string& node_id);

    bool add_peer_node(const std::shared_ptr<network::message>& msg);

    void remove_peer_node(const std::string& node_id);

    uint32_t get_peer_nodes_count_by_socket_type(network::socket_type type);


    void on_timer_check_peer_candidates(const std::shared_ptr<core_timer>& timer);

    void on_timer_dyanmic_adjust_network(const std::shared_ptr<core_timer>& timer);

    void on_timer_peer_info_exchange(const std::shared_ptr<core_timer>& timer);

    void on_timer_peer_candidate_dump(const std::shared_ptr<core_timer>& timer);


    uint32_t start_connect(const tcp::endpoint tcp_ep);


    void on_client_tcp_connect_notify(const std::shared_ptr<network::message>& msg);

    void on_tcp_channel_error(const std::shared_ptr<network::message>& msg);

    
    void add_dns_seeds();

    void add_ip_seeds();


    void send_ver_req(const tcp::endpoint& ep, const network::socket_id& sid);

    void on_ver_req(const std::shared_ptr<network::message>& msg);

    void send_ver_resp(const network::socket_id& sid);

    void on_ver_resp(const std::shared_ptr<network::message>& msg);


    void advertise_local(tcp::endpoint tcp_ep, network::socket_id sid);

    int32_t broadcast_peer_nodes(std::shared_ptr<peer_node> node);


    void on_broadcast_peer_nodes(const std::shared_ptr<network::message>& msg);


    int32_t get_available_peer_candidates(uint32_t count,
                                          std::vector<std::shared_ptr<peer_candidate>> &available_candidates);

    uint32_t get_available_peer_candidates_count_by_node_type(peer_node_type node_type = NORMAL_NODE);

    std::shared_ptr<peer_node> get_dynamic_disconnect_peer_node();

    std::shared_ptr<peer_candidate> get_dynamic_connect_peer_candidate();

    uint32_t get_rand32() { return m_rand_ctx.rand32(); }


    int32_t save_peer_candidates();

protected:
    uint256 m_rand_seed;
    FastRandomContext m_rand_ctx;

    std::string m_listen_ip;
    uint16_t m_listen_port = 10001;

    std::list<std::string> m_dns_seeds;
    std::list<std::string> m_ip_seeds;

    PeerCandidateDB m_peers_candidates_db;
     
    std::list<std::shared_ptr<peer_candidate>> m_peer_candidates;
    std::unordered_map<std::string, std::shared_ptr<peer_node>> m_peer_nodes_map;  // <node_id, ...>
};

#endif
