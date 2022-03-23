#ifndef DBC_NETWORK_ENDPOINT_ADDRESS_H
#define DBC_NETWORK_ENDPOINT_ADDRESS_H

#include "net_address.h"
#include <boost/asio.hpp>

using namespace boost::asio;

namespace network
{
    class endpoint_address : public net_address
    {
    public:
        endpoint_address() = default;

        virtual ~endpoint_address() = default;

        endpoint_address(const std::string &ip, uint16_t port) 
            : net_address(ip), m_port(port) {}

		endpoint_address(const ip::tcp::endpoint &ep) 
            : net_address(ep.address().to_string()), m_port(ep.port()) {}

		endpoint_address& operator=(const ip::tcp::endpoint &ep) 
		{
			m_ip = ep.address().to_string(); 
			m_port = ep.port();
			return *this;
		}

        bool operator==(const endpoint_address &addr)
        {
            return ((m_ip == addr.get_ip()) && (m_port == addr.get_port()));
        }

        uint16_t get_port() const { return m_port; }

    protected:
        uint16_t m_port;
    };
}

#endif
