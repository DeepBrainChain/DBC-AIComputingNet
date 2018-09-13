    /*********************************************************************************
    *  Copyright (c) 2017-2018 DeepBrainChain core team
    *  Distributed under the MIT software license, see the accompanying
    *  file COPYING or http://www.opensource.org/licenses/mit-license.php
    * file name        :   server_initiator.hpp
    * description    :   server initiator for server
    * date                  : 2017.08.02
    * author            :   Bruce Feng
    **********************************************************************************/

#pragma once

// #include "common.h"

namespace matrix
{
    namespace core
    {
        class server_initiator
        {
        public:

            virtual int32_t init(int argc, char* argv[]) = 0;

            virtual int32_t exit() = 0;

        };

    }

}




