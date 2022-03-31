#ifndef DBC_NETWORK_ENDPOINT_ADDRESS_H
#define DBC_NETWORK_ENDPOINT_ADDRESS_H

#include <boost/asio.hpp>

using namespace boost::asio;

namespace network
{
    class net_address
    {
    public:
        net_address() = default;

        net_address(const std::string &ip, uint16_t port)
            : m_ip(ip), m_port(port) {}

        net_address(const ip::tcp::endpoint &ep)
            : m_ip(ep.address().to_string()), m_port(ep.port()) {}

        net_address& operator=(const ip::tcp::endpoint &ep)
		{
			m_ip = ep.address().to_string(); 
			m_port = ep.port();
			return *this;
		}

        virtual ~net_address() = default;

        bool operator==(const net_address&addr)
        {
            return ((m_ip == addr.get_ip()) && (m_port == addr.get_port()));
        }

        std::string get_ip() const { return m_ip; }

        void set_ip(const std::string& ip) {
            m_ip = ip;
        }

        uint16_t get_port() const { return m_port; }

        void set_port(uint16_t port) {
            m_port = port;
        }

        static bool is_rfc1918(ip::tcp::endpoint tcp_ep);

    private:
        std::string m_ip;
        uint16_t m_port;
    };
}

#endif
