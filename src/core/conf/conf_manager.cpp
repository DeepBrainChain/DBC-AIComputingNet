/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºconf_manager.cpp
* description    £ºconf manager for core
* date                  : 2017.08.16
* author            £ºBruce Feng
**********************************************************************************/
#include "conf_manager.h"
#include "env_manager.h"
#include <boost/exception/all.hpp>

namespace matrix
{
    namespace core
    {
        int32_t conf_manager::init(bpo::variables_map &options)
        {
            //parse local conf
            int32_t ret = E_SUCCESS;
            
            ret = parse_local_conf();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "conf manager parse local conf error and exit";
                return ret;
            }

            ret = parse_node_dat();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "conf manager parse node dat error and exit";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t conf_manager::parse_local_conf()
        {
            //core opt description
            bpo::options_description core_opts("config file options");
            core_opts.add_options()
                ("host_ip", bpo::value<std::string>()->default_value("127.0.0.1"), "")
                ("main_net_listen_port", bpo::value<unsigned long>()->default_value(DEFAULT_MAIN_NET_LISTEN_PORT), "")
                ("test_net_listen_port", bpo::value<unsigned long>()->default_value(DEFAULT_TEST_NET_LISTEN_PORT), "");

            //peer opt description
            bpo::options_description peer_opts("peer options");
            peer_opts.add_options()
                ("peer", bpo::value<std::vector<std::string>>(), "");

            //file path
            const fs::path &conf_path = env_manager::get_conf_path();
            const fs::path &peer_path = env_manager::get_peer_path();

            try
            {
                //core.conf
                std::ifstream conf_ifs(conf_path.generic_string());
                bpo::store(bpo::parse_config_file(conf_ifs, core_opts), m_args);

                //peer.conf
                std::ifstream peer_ifs(peer_path.generic_string());
                bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "conf manager parse local conf error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t conf_manager::parse_node_dat()
        {
            //node dat description
            bpo::options_description node_dat_opts("node dat description");
            node_dat_opts.add_options()
                ("node_id", bpo::value<std::string>(), "")
                ("node_private_key", bpo::value<std::string>(), "");

            //node.dat path
            fs::path node_dat_path = env_manager::get_dat_path();
            node_dat_path /= fs::path(NODE_FILE_NAME);

            try
            {
                std::ifstream node_dat_ifs(node_dat_path.generic_string());
                bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "conf manager parse node.dat error: " << diagnostic_information(e);
                return E_DEFAULT;
            }
        }

        int32_t conf_manager::init_params()
        {
            assert(m_args.count("node_id") > 0);
            m_node_id = m_args["node_id"].as<std::string>();

            assert(m_args.count("node_private_key") > 0);
            m_node_private_key = m_args["node_private_key"].as<std::string>();
        }

    }

}