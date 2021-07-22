#ifndef DBC_NETWORK_NET_ADDRESS_H
#define DBC_NETWORK_NET_ADDRESS_H

#include <string>
#include <boost/asio.hpp>

using namespace boost::asio::ip;

namespace dbc
{
    namespace network
    {
        class net_address
        {
        public:

            net_address() = default;

            virtual ~net_address() = default;

            net_address(const std::string &ip) : m_ip(ip) {}

            const std::string get_ip() const { return m_ip; }

            static bool is_rfc1918(tcp::endpoint tcp_ep);

        protected:

            std::string m_ip;

        };
    }
}

#endif
