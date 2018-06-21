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


#include "common.h"


namespace matrix
{
    namespace core
    {

        struct peer_seeds
        {
            const char * seed;
            uint16_t port;
        };

        static peer_seeds g_main_peer_seeds[] = 
        {
            { "114.116.19.45", 11107 },
            { "114.116.21.175", 11107 },
            { "fe80::f816:3eff:fe44:afc9", 11107 },
            { "fe80::f816:3eff:fe33:d8db", 11107 }
        };

        static peer_seeds g_test_peer_seeds[] =
        {
            { "114.116.19.45", 21107 },
            { "114.116.21.175", 21107 },
            { "49.51.47.187", 21107 },
            { "49.51.47.174", 21107 },
            { "35.237.254.158", 21107 },
            { "35.227.90.8", 21107 },
            { "18.221.213.48", 21107 },
            { "18.188.157.102", 21107 },
            { "114.116.41.44", 21107 },
            { "114.115.219.202", 21107 },
            { "39.107.81.6", 21107 },
            { "47.93.24.54", 21107 },
            { "fe80::f816:3eff:fe44:afc9", 21107 },
            { "fe80::f816:3eff:fe33:d8db", 21107 }
        };

        static const char* g_main_dns_seeds[] = 
        {
            "seed.deepbrainchain.org"
        };

        static const char* g_test_dns_seeds[] =
        {
            "testnet-seed.deepbrainchain.org"
        };
    }

}
