/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         :   time_tick_notification.cpp
* description     :   time_tick_notification for all service module
* date                  :   2018.01.20
* author             :   Bruce Feng
**********************************************************************************/

#include "timer_matrix_manager.h"
#include "common.h"
#include "time_tick_notification.h"
#include "service_message.h"
#include "service_message_id.h"
#include "server.h"

namespace matrix
{
    namespace core
    {

        tick_type timer_matrix_manager::m_cur_tick = {0};

        timer_matrix_manager::timer_matrix_manager()
            : m_timer_group(new dbc::network::nio_loop_group())
        {

        }

        int32_t  timer_matrix_manager::init(bpo::variables_map &options)
        { 
            int ret = m_timer_group->init(DEFAULT_TIMER_MATRIX_THREAD_COUNT);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "timer matrix manager init timer thread group error";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t timer_matrix_manager::start() 
        { 
            m_timer = std::make_shared<steady_timer>(*(m_timer_group->get_io_service()));

            //auto self(shared_from_this());
            //m_timer_handler = [this, self](const boost::system::error_code & error)
            //{
            //    if (boost::asio::error::operation_aborted == error)
            //    {
            //        LOG_DEBUG << "timer matrix manager timer error: aborted";
            //        return;
            //    }
            //
            //    ++m_cur_tick;
            //
            //    //publish notification
            //    std::shared_ptr<message> msg = make_time_tick_notification();
            //    TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);
            //
            //    //next
            //    start_timer();
            //};

            //start thread
            int32_t ret = m_timer_group->start();
            if (E_SUCCESS != ret)
            {
                return ret;
            }

            //start timer
            start_timer();
            return E_SUCCESS;
        }

        int32_t  timer_matrix_manager::stop() 
        {
            stop_timer();
            m_timer_group->stop();
            //m_timer_handler = nullptr;
            LOG_DEBUG << "timer matrix manager has stopped";
            return E_SUCCESS;
        }

        int32_t  timer_matrix_manager::exit() 
        {
            return m_timer_group->exit();
        }

        void timer_matrix_manager::start_timer()
        {
            //assert(nullptr != m_timer_handler);
            
            //start tick timer
            m_timer->expires_from_now(std::chrono::milliseconds(DEFAULT_TIMER_INTERVAL));
            //m_timer->async_wait(m_timer_handler);
            m_timer->async_wait(boost::bind(&timer_matrix_manager::on_timer_expired,
                                            shared_from_this(), boost::asio::placeholders::error));

            //LOG_DEBUG << "timer matrix manager start timer: " << DEFAULT_TIMER_INTERVAL << "ms";
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
            TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);

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

    }

}