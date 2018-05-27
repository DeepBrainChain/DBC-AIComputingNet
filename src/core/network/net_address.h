/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   net_address.h
* description      :   p2p network address
* date             :   2018.03.29
* author           :   Bruce Feng
**********************************************************************************/

#pragma once


#include <string>
#include "common.h"


namespace matrix
{
    namespace core
    {

        class net_address
        {
        public:

            net_address() = default;

            virtual ~net_address() = default;

            net_address(const std::string &ip) : m_ip(ip) {}

            const std::string get_ip() const { return m_ip; }

        protected:

            std::string m_ip;

        };

    }

}