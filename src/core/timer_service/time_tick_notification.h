/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   time_tick_notification.h
* description    :   time_tick_notification for all service module
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include "protocol.h"


namespace matrix
{
    namespace core
    {

        class time_tick_notification : public matrix::core::base
        {
        public:

            uint64_t time_tick;

        };

    }

}
