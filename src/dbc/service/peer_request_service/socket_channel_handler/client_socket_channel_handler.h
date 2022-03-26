#pragma once

#include <boost/asio/steady_timer.hpp>
#include "tcp_socket_channel_handler.h"

using namespace boost::asio;

class matrix_client_socket_channel_handler : public matrix_socket_channel_handler
{
    typedef void (timer_handler_type)(const boost::system::error_code &);

public:
    matrix_client_socket_channel_handler(std::shared_ptr<network::channel> ch);

    virtual ~matrix_client_socket_channel_handler() = default;

    virtual int32_t start();

    static std::shared_ptr<socket_channel_handler> create(std::shared_ptr<network::channel> ch);

    virtual int32_t on_after_msg_received(network::message &msg);

    virtual int32_t on_after_msg_sent(network::message &msg);

protected:
    virtual void on_shake_hand_timer_expired(const boost::system::error_code& error);
    virtual void start_shake_hand_timer_ext();
    void send_shake_hand_req();

    virtual void start_wait_ver_resp_timer();
    virtual void stop_wait_ver_resp_timer();
    void on_ver_resp_timer_expired(const boost::system::error_code& error);

protected:
    steady_timer m_wait_ver_resp_timer;
    uint16_t m_lost_shake_hand_count;
    uint16_t m_lost_shake_hand_count_max;
};
