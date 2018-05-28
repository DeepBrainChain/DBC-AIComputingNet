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
    namespace service_core
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
            { "fe80::f816:3eff:fe44:afc9", 21107 },
            { "fe80::f816:3eff:fe33:d8db", 21107 }
        };

    }

}
