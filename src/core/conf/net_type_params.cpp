/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   net_type_param.h
* description    :   net type params for config, main net or test net
* date                  :   2018.05.26
* author             :   Bruce Feng
**********************************************************************************/


#include "net_type_params.h"
#include "common.h"
#include "server.h"
#include "peer_seeds.h"



namespace matrix
{
    namespace core
    {

        void net_type_params::init_seeds()
        {
            //init dns seeds
            const char **dns_seeds = (CONF_MANAGER->get_net_type() == MAIN_NET_TYPE) ? g_main_dns_seeds : g_test_dns_seeds;
            int dns_seeds_count = (CONF_MANAGER->get_net_type() == MAIN_NET_TYPE) ? (sizeof(g_main_dns_seeds) / sizeof(const char *)) : (sizeof(g_test_dns_seeds) / sizeof(const char *));
            assert(nullptr != dns_seeds);

            for (int i = 0; i < dns_seeds_count; i++)
            {
                LOG_DEBUG << "config add dns seed: " << dns_seeds[i];
                m_dns_seeds.push_back(dns_seeds[i]);
            }

            //init hard code seeds
            peer_seeds * hard_code_seeds = (CONF_MANAGER->get_net_type() == MAIN_NET_TYPE) ? g_main_peer_seeds : g_test_peer_seeds;
            assert(nullptr != hard_code_seeds);

            //get hard code seeds count
            int hard_code_seeds_count = (CONF_MANAGER->get_net_type() == MAIN_NET_TYPE) ? (sizeof(g_main_peer_seeds) / sizeof(peer_seeds)) : (sizeof(g_test_peer_seeds) / sizeof(peer_seeds));
            for (int i = 0; i < hard_code_seeds_count; i++)
            {
                LOG_DEBUG << "config add hard code seed ip: " << hard_code_seeds[i].seed << ", port: " << hard_code_seeds[i].port;
                m_hard_code_seeds.push_back(hard_code_seeds[i]);
            }
        }
    }

}