#include "multicast_reveiver.h"
#include "log/log.h"
#include "service/peer_request_service/p2p_lan_service.h"

multicast_receiver::multicast_receiver(boost::asio::io_context& io_context,
        const boost::asio::ip::address& listen_address,
        const boost::asio::ip::address& multicast_address,
        short multicast_port)
    : socket_(io_context) {
    // Create the socket so that multiple may be bound to the same address.
    boost::asio::ip::udp::endpoint listen_endpoint(
        listen_address, multicast_port);
    socket_.open(listen_endpoint.protocol());
    socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);

    // Join the multicast group.
    socket_.set_option(
        boost::asio::ip::multicast::join_group(multicast_address));
}

int32_t multicast_receiver::start() {
    do_receive();
    return 0;
}

int32_t multicast_receiver::stop() {
    return 0;
}

void multicast_receiver::do_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(data_), sender_endpoint_,
        [this](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            // std::cout.write(data_.data(), length);
            // std::cout << " from " << sender_endpoint_.address().to_string()
            //     << ":" << sender_endpoint_.port();
            // std::cout << std::endl;
            p2p_lan_service::instance().on_multicast_receive(
                std::string(data_.data(), length),
                sender_endpoint_.address().to_string());
            LOG_INFO << "receive multicast data from "
                     << sender_endpoint_.address().to_string()
                     << ":" << sender_endpoint_.port();

            do_receive();
          }
        });
}
