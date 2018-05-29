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


using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;

namespace matrix
{
    namespace service_core
    {

        class p2p_net_service : public service_module
        {

            using peer_list_type = typename std::list<std::shared_ptr<peer_node>>;
            using peer_map_type = typename std::unordered_map<std::string, std::shared_ptr<peer_node>>;

        public:

            p2p_net_service();

            virtual ~p2p_net_service() = default;

            virtual std::string module_name() const { return p2p_service_name; }

			virtual int32_t init(bpo::variables_map &options);

        public:

            //peer node
            void get_all_peer_nodes(peer_list_type &nodes);

            std::shared_ptr<peer_node> get_peer_node(const std::string &id);

            //config param
            std::string get_host_ip() const { return m_host_ip; }

            uint16_t get_net_listen_port() const {return m_net_listen_port;}


        protected:

            int32_t init_conf();

            int32_t init_acceptor();

            int32_t init_connector(bpo::variables_map &options);

            void init_subscription();

            void init_invoker();

            virtual void init_timer();

            //override by service layer
            virtual int32_t service_init(bpo::variables_map &options);

            int32_t service_exit();

            bool add_peer_node(const socket_id &sid, const std::string &nid, int32_t core_version, int32_t protocol_version);

            void remove_peer_node(const std::string &id);

        protected:

            int32_t on_ver_req(std::shared_ptr<message> &msg);

            int32_t on_ver_resp(std::shared_ptr<message> &msg);

            int32_t on_client_tcp_connect_notification(std::shared_ptr<message> &msg);

            int32_t on_tcp_channel_error(std::shared_ptr<message> &msg);

            //cmd get peer nodes
            int32_t on_cmd_get_peer_nodes_req(std::shared_ptr<message> &msg);

			int32_t on_get_peer_nodes_req(std::shared_ptr<message> &msg);

			int32_t on_get_peer_nodes_resp(std::shared_ptr<message> &msg);

        protected:
            //active pull
            int32_t send_get_peer_nodes();

            //active push
            int32_t send_put_peer_nodes(std::shared_ptr<peer_node> node);

            int32_t on_timer_one_minute(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_timer_peer_info_exchange();

            int32_t on_timer_check_peer_candidate();

            int32_t on_timer_peer_candidate_dump();

        protected:

            std::string m_host_ip;

            uint16_t m_net_listen_port;

            std::list<peer_candidate> m_peer_candidates;

            rw_lock m_nodes_lock;

            peer_map_type m_peer_nodes_map;

            uint32_t m_timer_id_one_minute;

            std::list<const char *> m_dns_seeds;

            std::list<peer_seeds> m_hard_code_seeds;

        };

    }

}
