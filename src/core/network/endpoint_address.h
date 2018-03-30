/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºendpoint_address.h
* description    £ºp2p network endpoint address
* date                  : 2018.03.29
* author            £ºBruce Feng
**********************************************************************************/

#pragma once

#include "net_address.h"


namespace matrix
{
    namespace core
    {

        class endpoint_address : public net_address
        {
        public:

            endpoint_address() = default;

            virtual ~endpoint_address() = default;

            endpoint_address(const std::string &ip, uint16_t port) : net_address(ip), m_port(port) {}

            uint16_t get_port() const { return m_port; }

        protected:

            uint16_t m_port;
        };

    }

}
