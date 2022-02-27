#include "timer_matrix_manager.h"
#include "time_tick_notification.h"
#include "network/protocol/service_message.h"
#include "service_module/service_message_id.h"
#include "server/server.h"

std::atomic<std::uint64_t> timer_matrix_manager::m_cur_tick = {0};

ERRCODE timer_matrix_manager::init()
{
    m_timer_group = std::make_shared<dbc::network::nio_loop_group>();

    int32_t ret = m_timer_group->init(DEFAULT_TIMER_MATRIX_THREAD_COUNT);
    if (ERR_SUCCESS != ret)
    {
        LOG_ERROR << "timer matrix manager init timer thread group error";
        return ret;
    }

	m_timer = std::make_shared<steady_timer>(*(m_timer_group->get_io_service()));

    ret = m_timer_group->start();
	if (ERR_SUCCESS != ret)
	{
		return ret;
	}

	start_timer();

    return ERR_SUCCESS;
}

void timer_matrix_manager::exit()
{
    stop_timer();
    m_timer_group->stop();
    //m_timer_handler = nullptr;
    m_timer_group->exit();
}

void timer_matrix_manager::start_timer()
{
    //start tick timer
    m_timer->expires_from_now(std::chrono::milliseconds(DEFAULT_TIMER_INTERVAL));
    //m_timer->async_wait(m_timer_handler);
    m_timer->async_wait(boost::bind(&timer_matrix_manager::on_timer_expired,
                                    shared_from_this(), boost::asio::placeholders::error));
}

void timer_matrix_manager::on_timer_expired(const boost::system::error_code& error)
{
    if (boost::asio::error::operation_aborted == error)
    {
        LOG_DEBUG << "timer matrix manager timer error: aborted";
        return;
    }

    ++m_cur_tick;

    //publish notification
    std::shared_ptr<dbc::network::message> msg = make_time_tick_notification();
    topic_manager::instance().publish<void>(msg->get_name(), msg);

    //next
    start_timer();
}

void timer_matrix_manager::stop_timer()
{
    boost::system::error_code error;

    //cancel tick timer
    m_timer->cancel(error);
    if (error)
    {
        LOG_ERROR << "timer matrix manager cancel timer error: " << error;
    }
    else
    {
        LOG_DEBUG << "timer matrix manager stop timer";
    }
}

std::shared_ptr<dbc::network::message> timer_matrix_manager::make_time_tick_notification()
{
    //notification
    std::shared_ptr<time_tick_notification> content(new time_tick_notification);
    content->time_tick = m_cur_tick;

    //message
    std::shared_ptr<dbc::network::message> msg = std::make_shared<dbc::network::message>();
    msg->set_content(content);
    msg->set_name(TIMER_TICK_NOTIFICATION);

    return msg;
}
