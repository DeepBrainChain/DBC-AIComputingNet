#include "client_socket_channel_handler.h"

matrix_client_socket_channel_handler::matrix_client_socket_channel_handler(
    std::shared_ptr<network::channel> ch)
    : matrix_socket_channel_handler(ch)
    , m_lost_shake_hand_count(0)
    , m_wait_ver_resp_timer(*(ch->get_io_service()))
    , m_lost_shake_hand_count_max(LOST_SHAKE_HAND_COUNT_MAX)
{

}

int32_t matrix_client_socket_channel_handler::start()
{
    return ERR_SUCCESS;
}


void matrix_client_socket_channel_handler::start_shake_hand_timer_ext()
{
    m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
    m_shake_hand_timer.async_wait(boost::bind(&matrix_client_socket_channel_handler::on_shake_hand_timer_expired,
        std::dynamic_pointer_cast<matrix_client_socket_channel_handler>(shared_from_this()), boost::asio::placeholders::error));
}


void matrix_client_socket_channel_handler::on_shake_hand_timer_expired(const boost::system::error_code& error)
{

    if (true == m_stopped)
    {
        LOG_WARNING << "matrix_client_socket_channel_handler has been stopped and shake_hand_timer_handler exit directly" << m_sid.to_string();
        return;
    }

    if (error)
    {
        //aborted, maybe cancel triggered
        if (boost::asio::error::operation_aborted == error.value())
        {
            LOG_DEBUG << "matrix client socket channel handler timer aborted.";
            return;
        }

        LOG_ERROR << "matrix client socket channel handler timer error: " << error.value() << " " << error.message() << m_sid.to_string();
        if (auto ch = m_channel.lock())
        {
            ch->on_error();
        }
        return;
    }

    //login success and no message, time out and send shake hand
    if (true == m_login_success && false == m_has_message)
    {
        send_shake_hand_req();
    }

    //reset
    reset_has_message();

    //next time async wait

    //lost shake hand too many times
    /*if (++m_lost_shake_hand_count >= m_lost_shake_hand_count_max)
    {
        LOG_ERROR << "matrix client socket channel handler lost shake hand count error timers: " << m_lost_shake_hand_count;
        if (auto ch = m_channel.lock())
        {
            ch->on_error();
        }
        return;
    }*/

    start_shake_hand_timer_ext();
}

int32_t matrix_client_socket_channel_handler::on_after_msg_received(network::message &msg)
{
    if (false == m_login_success && VER_RESP != msg.get_name())
    {
        LOG_ERROR << "matrix client socket channel received error message: " << msg.get_name() << " , while not login success" << msg.header.src_sid.to_string();
        /*if (auto ch = m_channel.lock())
        {
            ch->on_error();
        }
        return ERR_SUCCESS;*/
        return E_DEFAULT;
    }

    if (VER_RESP == msg.get_name())
    {
        if (false == m_login_success)
        {
            m_login_success = true;
            stop_wait_ver_resp_timer();
            start_shake_hand_timer();

            LOG_DEBUG << "matrix client socket channel handler start shake hand timer, " << m_sid.to_string();
            return ERR_SUCCESS;
        }
        else
        {
            LOG_ERROR << "matrix client socket channel handler received duplicated VER_RESP" << m_sid.to_string();
            /*if (auto ch = m_channel.lock())
            {
                ch->on_error();
            }*/
            return E_DEFAULT;
        }
    }
    m_lost_shake_hand_count = 0;
    return ERR_SUCCESS;
}

int32_t matrix_client_socket_channel_handler::on_after_msg_sent(network::message &msg)
{
    if (VER_REQ == msg.get_name())
    {
        start_wait_ver_resp_timer();
        LOG_DEBUG << "matrix client socket channel handler start ver_resp timer, " << m_sid.to_string();
        return ERR_SUCCESS;
    }

    if (SHAKE_HAND_REQ == msg.get_name())
    {
        return ERR_SUCCESS;
    }

    m_lost_shake_hand_count = 0;
    return ERR_SUCCESS;
}

std::shared_ptr<network::socket_channel_handler> matrix_client_socket_channel_handler::create(std::shared_ptr<network::channel> ch)
{
    std::shared_ptr<socket_channel_handler> handler = std::make_shared<matrix_client_socket_channel_handler>(ch);
    return handler->shared_from_this();
}

void matrix_client_socket_channel_handler::send_shake_hand_req()
{
    //shake_hand_req
    std::shared_ptr<network::message> req_msg = std::make_shared<network::message>();
    std::shared_ptr<dbc::shake_hand_req> req_content = std::make_shared<dbc::shake_hand_req>();

    //header
    req_content->header.__set_magic(ConfManager::instance().GetNetFlag());
    req_content->header.__set_msg_name(SHAKE_HAND_REQ);

    req_msg->set_content(req_content);
    req_msg->set_name(SHAKE_HAND_REQ);

    if (auto ch = m_channel.lock())
    {
        ch->write(req_msg);
    }
}

void matrix_client_socket_channel_handler::start_wait_ver_resp_timer()
{
    m_wait_ver_resp_timer.expires_from_now(std::chrono::seconds(DEFAULT_WAIT_VER_REQ_INTERVAL));
    m_wait_ver_resp_timer.async_wait(boost::bind(&matrix_client_socket_channel_handler::on_ver_resp_timer_expired,
        std::dynamic_pointer_cast<matrix_client_socket_channel_handler>(shared_from_this()), boost::asio::placeholders::error));

    LOG_DEBUG << "matrix client socket channel handler start wait ver resp timer, " << m_sid.to_string();
}

void matrix_client_socket_channel_handler::stop_wait_ver_resp_timer()
{
    boost::system::error_code error;

    m_wait_ver_resp_timer.cancel(error);
    if (error)
    {
        LOG_ERROR << "matrix client socket channel handler cancel timer error: " << error;
    }
    else
    {
        LOG_DEBUG << "matrix client socket channel handler stop wait ver req timer, " << m_sid.to_string();
    }
}

void matrix_client_socket_channel_handler::on_ver_resp_timer_expired(const boost::system::error_code& error)
{
    if (true == m_stopped)
    {
        LOG_DEBUG << "matrix_server_socket_channel_handler has been stopped and on_ver_req_timer_expired exit directly" << m_sid.to_string();
        return;
    }

    if (error)
    {
        //aborted, maybe cancel triggered
        if (boost::asio::error::operation_aborted == error.value())
        {
            LOG_DEBUG << "matrix client socket channel handler wait ver req timer aborted.";
            return;
        }

        LOG_ERROR << "matrix client socket channel handler wait ver req timer error: " << error.value() << " " << error.message() << m_sid.to_string();
        if (auto ch = m_channel.lock())
        {
            ch->on_error();
        }
        return;
    }

    //time out and disconnect tcp socket
    if (m_wait_ver_resp_timer.expires_at() < std::chrono::steady_clock::now())
    {
        LOG_ERROR << "wait ver_req_resp failed, " << m_sid.to_string();

        //stop_wait_ver_req_timer();
        if (auto ch = m_channel.lock())
        {
            ch->on_error();
        }
        return;
    }

    //restart timer for not time out
    start_wait_ver_resp_timer();
}
