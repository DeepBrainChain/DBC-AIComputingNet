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
    }
}
