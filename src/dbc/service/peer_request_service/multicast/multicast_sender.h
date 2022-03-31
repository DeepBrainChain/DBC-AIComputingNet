#ifndef DBC_MULTICAST_SENDER_H
#define DBC_MULTICAST_SENDER_H

#include <string>
#include <boost/asio.hpp>

class multicast_sender {
public:
    multicast_sender() = delete;
    multicast_sender(boost::asio::io_context& io_context,
        const boost::asio::ip::address& multicast_address,
        short multicast_port);
    ~multicast_sender() = default;
    
    void send(const std::string& message);
    
protected:
private:
    boost::asio::ip::udp::endpoint endpoint_;
    boost::asio::ip::udp::socket socket_;
    std::string message_;
};

#endif
