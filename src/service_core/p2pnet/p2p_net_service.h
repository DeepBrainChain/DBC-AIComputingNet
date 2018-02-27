/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºp2p_net_service.h
* description    £ºp2p net service
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include "service_module.h"
#include <boost/asio.hpp>
#include <string>
#include "handler_create_functor.h"

using namespace matrix::core;
using namespace boost::asio::ip;

namespace matrix
{
    namespace service_core
    {

        class p2p_net_service : public service_module
        {
        public:

            p2p_net_service() = default;

            virtual ~p2p_net_service() = default;

            virtual std::string module_name() const { return p2p_manager_name; }

            std::string get_host_ip() const { return m_host_ip; }

            uint16_t get_main_net_listen_port() const {return m_main_net_listen_port;}

            uint16_t get_test_net_listen_port() const { return m_test_net_listen_port; }

        protected:

            void init_conf();

            void init_acceptor();

            void init_connector();

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

        };

    }

}

