#pragma once

#include <boost/asio/steady_timer.hpp>
#include "network/channel/channel.h"
#include "network/channel/tcp_socket_channel.h"
#include "network/channel/socket_channel_handler.h"
#include "service/message/protocol_coder/matrix_coder.h"
#include "server/server.h"
#include "service_module/service_message_id.h"
#include "network/utils/flow_ctrl.h"

using namespace boost::asio;

#define SHAKE_HAND_INTERVAL                     5  //shake hand interval
#define LOST_SHAKE_HAND_COUNT_MAX               3  //max lost shake hand req
#define DEFAULT_WAIT_VER_REQ_INTERVAL           5  //wait VER_REQ interval while socket accepted

class msg_new_peer_node : public network::msg_base
{
public:
    network::socket_id  sid;
    std::string     node_id;
};

class matrix_socket_channel_handler : public network::socket_channel_handler
{
public:
    typedef void (timer_handler_type)(const boost::system::error_code &);

    matrix_socket_channel_handler(std::shared_ptr<network::channel> ch);

    virtual ~matrix_socket_channel_handler();

public:

    virtual int32_t start() { return ERR_SUCCESS; }

    virtual int32_t stop();

    virtual int32_t on_read(network::channel_handler_context &ctx, byte_buf &in);

    virtual int32_t on_write(network::channel_handler_context &ctx, network::message &msg, byte_buf &buf);

    virtual int32_t on_error();

    virtual int32_t on_before_msg_send(network::message &msg) { return ERR_SUCCESS; }

    virtual int32_t on_after_msg_sent(network::message &msg) { return ERR_SUCCESS; }

    virtual int32_t on_after_msg_received(network::message &msg) { return ERR_SUCCESS; }

    virtual int32_t on_before_msg_receive() { return ERR_SUCCESS; }

    virtual bool is_logined() { return m_login_success; }

    virtual void on_shake_hand_timer_expired(const boost::system::error_code& error) {}

protected:
    virtual void start_shake_hand_timer();

    virtual void stop_shake_hand_timer();

    virtual void start_shake_hand_timer_ext() {}

    void set_has_message(network::message &msg);

    void reset_has_message() { m_has_message = false; }

    void set_encode_context(network::channel_handler_context &ctx);

    void set_decode_context(network::channel_handler_context &ctx);

private:
    bool validate_req_path(std::string msg_name, std::vector<std::string>& path);
    bool validate_resp_path(std::string msg_name, std::vector<std::string>& path);

protected:
    bool m_stopped;

    std::shared_ptr<matrix_coder> m_coder;
    //fix coding and decoding conflict when p2p communication
    std::shared_ptr<matrix_coder> m_decoder;

    std::weak_ptr<network::channel> m_channel;
    steady_timer m_shake_hand_timer;

    //std::function<timer_handler_type> m_shake_hand_timer_handler;
    bool m_has_message;
    bool m_login_success;

    network::socket_id m_sid;
    std::shared_ptr<network::flow_ctrl> m_f_ctl = nullptr;
};
