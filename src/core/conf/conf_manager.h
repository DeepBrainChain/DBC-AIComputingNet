/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   conf_manager.h
* description    :   conf manager for core
* date                  : 2017.08.16
* author            :   Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"
#include "module.h"
#include "id_generator.h"
#include "net_type_params.h"


using namespace boost::program_options;



#define NODE_FILE_NAME                                                   "node.dat"
#define DEFAULT_MAIN_NET_LISTEN_PORT                 "11107"
#define DEFAULT_TEST_NET_LISTEN_PORT                  "21107"


extern std::string DEFAULT_CONTAINER_LISTEN_PORT;
extern std::string DEFAULT_CONTAINER_IMAGE_NAME;      
extern const int32_t DEFAULT_MAX_CONNECTION_NUM;
extern const int32_t DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND;
extern const int32_t DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND;
extern const int32_t DEFAULT_SPEED;

extern const std::string conf_manager_name;

namespace matrix
{
    namespace core
    {

        class conf_manager : public module
        {

        public:

            conf_manager();

            virtual ~conf_manager() = default;

            virtual std::string module_name() const {return conf_manager_name;}

            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t exit() { m_args.clear(); return E_SUCCESS; }

        public:       

            int32_t get_log_level() { return m_log_level; }

            int32_t get_net_flag() { return m_net_flag; }

            const std::string & get_net_type() { return m_net_type; }

            uint16_t get_net_default_port();

            const std::string & get_net_listen_port() { return m_net_params->get_net_listen_port(); }

            const std::string & get_host_ip() {return m_args.count("host_ip") ? m_args["host_ip"].as<std::string>() : DEFAULT_IP_V4;}

            const std::vector<std::string> & get_peers() { return m_args.count("peer") ? m_args["peer"].as<std::vector<std::string>>() : DEFAULT_VECTOR; }

            const std::vector<const char *> &get_dns_seeds() { return m_net_params->get_dns_seeds(); }

            const std::vector<peer_seeds> &get_hard_code_seeds() { return m_net_params->get_hard_code_seeds(); }

            const std::string & get_container_ip() { return m_args.count("container_ip") ? m_args["container_ip"].as<std::string>() : DEFAULT_LOCAL_IP; }

            const std::string & get_container_port() { return m_args.count("container_port") ? m_args["container_port"].as<std::string>() : DEFAULT_CONTAINER_LISTEN_PORT; }

            const int32_t & get_max_connect() {return m_args.count("max_connect") ? m_args["max_connect"].as<int32_t>() : DEFAULT_MAX_CONNECTION_NUM;}

            const int32_t & get_timer_service_broadcast_in_second() {
                return m_args.count("timer_service_broadcast_in_second") ? m_args["timer_service_broadcast_in_second"].as<int32_t>() : DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND;
            }

            const int32_t & get_timer_service_list_expired_in_second() {
                return m_args.count("timer_service_list_expired_in_second") ? m_args["timer_service_list_expired_in_second"].as<int32_t>() : DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND;
            }


            //const std::string & get_container_image() { return m_args.count("container_image") ? m_args["container_image"].as<std::string>() : DEFAULT_CONTAINER_IMAGE_NAME; }

            const std::string & get_node_id() const { return m_node_id; }

            const std::string & get_node_private_key() const {return m_node_private_key;}

            static int32_t serialize_node_info(const node_info &info);

            const std::string & get_bill_url() 
            {
                return  m_args.count("bill_url") > 0 ?  m_args["bill_url"].as<std::string>():DEFAULT_STRING;
            }

            const std::string & get_bill_crt()
            {
                return m_args.count("bill_crt") > 0 ? m_args["bill_crt"].as<std::string>() : DEFAULT_STRING;
            }

            const int32_t & get_max_recv_speed()
            {
                return m_args.count("max_recv_speed") > 0 ? m_args["max_recv_speed"].as<int32_t>() : DEFAULT_SPEED;
            }

            std::string get_dbc_path() { return m_dbc_path;}

        protected:

            int32_t parse_local_conf();

            int32_t parse_node_dat();

            int32_t init_params();

            void init_net_flag();

            const variable_value& operator[](const std::string& name) const { return m_args[name]; }

            bool count(const std::string& name) const { return m_args.count(name);}

            int32_t gen_new_nodeid();

            bool check_node_info();

        protected:

            variables_map m_args;

            std::string m_node_id;

            std::string m_node_private_key;

            std::string m_net_type;

            int32_t m_net_flag;

            int32_t m_log_level;

            //params relative with net type
            std::shared_ptr<net_type_params> m_net_params;

            std::string m_dbc_path;
        };

    }

}




