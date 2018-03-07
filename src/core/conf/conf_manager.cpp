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
            return parse_local_conf();
        }

        int32_t conf_manager::parse_local_conf()
        {
            //core opt description
            bpo::options_description core_opts("config file options");
            core_opts.add_options()
                ("host_ip", bpo::value<std::string>()->default_value("127.0.0.1"), "")
                ("main_net_listen_port", bpo::value<uint16_t>()->default_value(DEFAULT_MAIN_NET_LISTEN_PORT), "")
                ("test_net_listen_port", bpo::value<uint16_t>()->default_value(DEFAULT_TEST_NET_LISTEN_PORT), "");

            //peer opt description
            bpo::options_description peer_opts("peer options");
            peer_opts.add_options()
                ("peer", bpo::value<std::vector<std::string>>(), "");

            //file path
            const std::string &conf_path = env_manager::get_conf_path().generic_string();
            const std::string &peer_path = env_manager::get_peer_path().generic_string();

            try
            {
                //core.conf
                std::ifstream conf_ifs(conf_path);
                bpo::store(bpo::parse_config_file(conf_ifs, core_opts), m_args);

                //peer.conf
                std::ifstream peer_ifs(peer_path);
                bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "conf mananger parse local conf error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

    }

}