#ifndef DBC_MULTICAST_RECEIVER_H
#define DBC_MULTICAST_RECEIVER_H

#include <array>
#include <string>
#include <boost/asio.hpp>

class multicast_receiver {
public:
    multicast_receiver() = delete;
    multicast_receiver(boost::asio::io_context& io_context,
        const boost::asio::ip::address& listen_address,
        const boost::asio::ip::address& multicast_address,
        short multicast_port);
    ~multicast_receiver() = default;
    
    int32_t start();

    int32_t stop();

protected:
    void do_receive();

private:
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint sender_endpoint_;
    std::array<char, 1024> data_;
};

#endif
