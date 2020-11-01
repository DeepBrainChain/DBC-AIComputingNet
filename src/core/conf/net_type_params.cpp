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
        static peer_seeds g_main_peer_seeds[] = 
        {
            { "111.44.254.139", 12112 },
            { "111.44.254.140", 12112 },
            { "114.116.41.44", 12112 },
         //  { "114.116.21.175", 11111 },
         //   { "114.115.219.202", 11111 },
         //  { "111.44.254.141", 11111 },


        };

        static peer_seeds g_test_peer_seeds[] =
        {

            { "114.116.21.175", 21107 },
            { "114.116.41.44", 21107 },
            { "114.115.219.202", 21107 }


            //{ "39.107.81.6", 21107 },
            //{ "47.93.24.54", 21107 }
            //{ "fe80::f816:3eff:fe44:afc9", 21107 },
            //{ "fe80::f816:3eff:fe33:d8db", 21107 }
        };

        static const char* g_main_dns_seeds[] = 
        {
            "seed.dbchain.ai",
            "seed.deepbrainchain.org"
        };

        static const char* g_test_dns_seeds[] =
        {
            "testnet-seed.dbccloud.io",
            "testnet-seed.deepbrainchain.org"
        };  


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