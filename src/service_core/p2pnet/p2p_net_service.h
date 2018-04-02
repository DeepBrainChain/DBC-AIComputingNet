/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name      p2p_net_service.h
* description    p2p net service
* date                  : 2018.01.28
* author            Bruce Feng
**********************************************************************************/
#pragma once


#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include "service_module.h"
#include "handler_create_functor.h"
#include "peer_node.h"


using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;

#define DEFAULT_CONNECT_PEER_NODE      102400                            //default connect peer nodes

namespace matrix
{
    namespace service_core
    {

        class p2p_net_service : public service_module
        {

            using peer_list_type = typename std::list<std::shared_ptr<peer_node>>;
            using peer_map_type = typename std::unordered_map<std::string, std::shared_ptr<peer_node>>;

        public:

            p2p_net_service() = default;

            virtual ~p2p_net_service() = default;

            virtual std::string module_name() const { return p2p_manager_name; }

        public:

            //peer node
            void get_all_peer_nodes(peer_list_type &nodes);

            std::shared_ptr<peer_node> get_peer_node(const std::string &id);

            //config param
            std::string get_host_ip() const { return m_host_ip; }

            uint16_t get_main_net_listen_port() const {return m_main_net_listen_port;}

            uint16_t get_test_net_listen_port() const { return m_test_net_listen_port; }

        protected:

            int32_t init_conf();

            int32_t init_acceptor();

            int32_t init_connector();

            void init_subscription();

            void init_invoker();

            //override by service layer
            virtual int32_t service_init(bpo::variables_map &options);

            virtual int32_t on_time_out(std::shared_ptr<core_timer> timer);

        protected:

            int32_t on_ver_req(std::shared_ptr<message> &msg);

            int32_t on_ver_resp(std::shared_ptr<message> &msg);

            int32_t on_client_tcp_connect_notification(std::shared_ptr<message> &msg);

            int32_t on_tcp_channel_error(std::shared_ptr<message> &msg);

        protected:

            std::string m_host_ip;

            uint16_t m_main_net_listen_port;

            uint16_t m_test_net_listen_port;

            std::list<tcp::endpoint> m_peer_addresses;

            rw_lock m_nodes_lock;

            //peer_list_type m_peer_nodes;

            peer_map_type m_peer_nodes_map;

        };

    }

}
