#include "multicast_sender.h"
#include "log/log.h"

multicast_sender::multicast_sender(boost::asio::io_context& io_context,
        const boost::asio::ip::address& multicast_address,
        short multicast_port)
    : endpoint_(multicast_address, multicast_port)
    , socket_(io_context, endpoint_.protocol()) {

}

void multicast_sender::send(const std::string& message) {
    try {
        message_ = message;

        socket_.async_send_to(
            boost::asio::buffer(message_), endpoint_,
            [this](boost::system::error_code ec, std::size_t /*length*/)
            {
            if (!ec)
                LOG_INFO << "send multicast message success";
            });
    }
    catch (const std::exception & e)
    {
        LOG_ERROR << "send multicast message Exception: " << e.what();
    }
}
