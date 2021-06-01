/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name            :    p2p_net_service.h
* description        :    p2p net service
* date                      :    2018.01.28
* author                 :    Bruce Feng
**********************************************************************************/
#pragma once


#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include "service_module.h"
#include "handler_create_functor.h"
#include "peer_node.h"
#include "peer_candidate.h"
#include "peer_seeds.h"
#include "service_common_def.h"
#include "random.h"
#include <leveldb/db.h>


using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;


#define MIN_PEER_CANDIDATES_COUNT                8
#define MAX_SEND_PEER_NODES_COUNT                16
#define MIN_NORMAL_AVAILABLE_NODE_COUNT          2
#define DISCONNECT_NODE_PER_MINUTES              5


namespace matrix {
    namespace service_core {

        class p2p_net_service : public service_module {
            using peer_list_type = typename std::list<std::shared_ptr<peer_node>>;
            using peer_map_type = typename std::unordered_map<std::string, std::shared_ptr<peer_node>>;

        public:
            p2p_net_service();

            virtual ~p2p_net_service() = default;

            virtual std::string module_name() const { return p2p_service_name; }

            //config param
            std::string get_host_ip() const { return m_host_ip; }

            uint16_t get_net_listen_port() const { return m_net_listen_port; }

        protected:
            int32_t init_rand();

            int32_t init_conf();

            int32_t init_db();

            int32_t init_acceptor();

            int32_t init_connector(bpo::variables_map &options);

            void init_subscription();

            void init_invoker();

            virtual void init_timer();

            //override by service layer
            virtual int32_t service_init(bpo::variables_map &options);

            int32_t service_exit();

            //if call from outside, please think about thread-safe of m_peer_nodes_map
            void get_all_peer_nodes(peer_list_type &nodes);

            std::shared_ptr<peer_node> get_peer_node(const std::string &id);

            bool add_peer_node(std::shared_ptr<message> &msg);

            void remove_peer_node(const std::string &id);

            bool exist_peer_node(tcp::endpoint ep);

            bool exist_peer_node(std::string &nid);

            uint32_t get_peer_nodes_count_by_socket_type(socket_type type);

            int32_t on_ver_req(std::shared_ptr<message> &msg);

            int32_t on_ver_resp(std::shared_ptr<message> &msg);

            int32_t on_client_tcp_connect_notification(std::shared_ptr<message> &msg);

            int32_t on_tcp_channel_error(std::shared_ptr<message> &msg);

            int32_t on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg);

            int32_t on_get_peer_nodes_req(std::shared_ptr<message> &msg);

            int32_t on_get_peer_nodes_resp(std::shared_ptr<message> &msg);

            //active pull
            int32_t send_get_peer_nodes();

            //active push
            int32_t send_put_peer_nodes(std::shared_ptr<peer_node> node);

            int32_t on_timer_check_peer_candidates(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_timer_dyanmic_adjust_network(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_timer_peer_info_exchange(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_timer_peer_candidate_dump(std::shared_ptr<matrix::core::core_timer> timer);

            uint32_t get_rand32() { return m_rand_ctx.rand32(); }

            uint32_t get_maybe_available_peer_candidates_count();

            int32_t get_available_peer_candidates(uint32_t count,
                                                  std::vector<std::shared_ptr<peer_candidate>> &available_candidates);

            uint32_t get_available_peer_candidates_count_by_node_type(peer_node_type node_type = NORMAL_NODE);

            bool is_peer_candidate_exist(tcp::endpoint &ep);

            bool add_peer_candidate(tcp::endpoint &ep, net_state ns, peer_node_type ntype, std::string node_id = "");

            bool update_peer_candidate_state(tcp::endpoint &ep, net_state ns);

            int32_t load_peer_candidates();

            int32_t save_peer_candidates();

            int32_t clear_peer_candidates_db();

            uint32_t start_connect(const tcp::endpoint tcp_ep);

            std::shared_ptr<peer_node> get_dynamic_disconnect_peer_node();

            std::shared_ptr<peer_candidate> get_dynamic_connect_peer_candidate();

            std::shared_ptr<peer_candidate> get_peer_candidate(const tcp::endpoint &ep);

            void remove_peer_candidate(const tcp::endpoint &ep);

            int32_t add_dns_seeds();

            int32_t add_hard_code_seeds();

            void advertise_local(tcp::endpoint tcp_ep, socket_id sid);

        protected:
            uint256 m_rand_seed;

            FastRandomContext m_rand_ctx;

            std::string m_host_ip;

            uint16_t m_net_listen_port;

            std::list<std::shared_ptr<peer_candidate>> m_peer_candidates;

            std::unordered_map<std::string, std::shared_ptr<peer_node>> m_peer_nodes_map;

            std::list<const char *> m_dns_seeds;

            std::list<peer_seeds> m_hard_code_seeds;

            uint32_t m_timer_check_peer_candidates;

            uint32_t m_timer_dyanmic_adjust_network;

            uint32_t m_timer_peer_info_exchange;

            uint32_t m_timer_dump_peer_candidates;

            std::shared_ptr<leveldb::DB> m_peers_db;
        };
    }
}
