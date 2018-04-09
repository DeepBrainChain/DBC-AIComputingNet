/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtime_point_notification.h
* description    £ºtime_point_notification for all service module
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/

#pragma once


#include "protocol.h"


namespace matrix
{
    namespace core
    {

        class time_point_notification : public matrix::core::base
        {
        public:

            uint64_t time_tick;

        };

    }

}
