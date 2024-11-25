#ifndef DBC_DEEPLINK_SESSION_H
#define DBC_DEEPLINK_SESSION_H

#include <boost/asio.hpp>
#include <deque>

#include "deeplink_participant.h"

class deeplink_room;

class deeplink_session : public deeplink_participant,
                         public std::enable_shared_from_this<deeplink_session> {
public:
    deeplink_session(boost::asio::ip::tcp::socket socket, deeplink_room& room);
    ~deeplink_session();

    void start();
    void close();
    void deliver(const deeplink_lan_message& msg);

    void get_device_info(std::string& device_id, std::string& password);
    void set_device_info(const std::string& device_id,
                         const std::string& password);

private:
    void do_read_header();
    void do_read_body();
    void do_write();

    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::ip::tcp::socket socket_;
    deeplink_room& room_;
    deeplink_lan_message read_msg_;
    std::deque<deeplink_lan_message> write_msgs_;

    std::string device_id_;
    std::string password_;
};

#endif  // DBC_DEEPLINK_SESSION_H
