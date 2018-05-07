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
#include "boost/asio.hpp"


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

			endpoint_address(const boost::asio::ip::tcp::endpoint &ep) : net_address(ep.address().to_string()), m_port(ep.port()) {}

			endpoint_address& operator=(const boost::asio::ip::tcp::endpoint &ep) 
			{
				m_ip = ep.address().to_string(); 
				m_port = ep.port();
				return *this;
			}

            uint16_t get_port() const { return m_port; }

        protected:

            uint16_t m_port;
        };

    }

}
