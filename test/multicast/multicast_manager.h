#ifndef DBC_MULTICAST_MANAGER_H
#define DBC_MULTICAST_MANAGER_H

#include <array>
#include <string>
#include <boost/asio.hpp>

class multicast_manager {
public:
    multicast_manager() = delete;
    multicast_manager(boost::asio::io_context& io_context,
        const boost::asio::ip::address& listen_address,
        const boost::asio::ip::address& multicast_address,
        short multicast_port);
    ~multicast_manager() = default;

    void do_receive();

    void do_send();
    void do_timeout();
private:
    boost::asio::ip::udp::socket receiver_socket_;
    boost::asio::ip::udp::socket sender_socket_;
    boost::asio::ip::udp::endpoint sender_endpoint_;
    boost::asio::ip::udp::endpoint endpoint_;
    std::array<char, 1024> data_;
    boost::asio::steady_timer timer_;
    int message_count_;
    std::string message_;
    std::string node_id_;
};

#endif
