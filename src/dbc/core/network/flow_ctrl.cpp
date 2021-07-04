#include "flow_ctrl.h"

namespace dbc
{
    namespace network
    {
        flow_ctrl::flow_ctrl(int32_t max_num, int32_t cycle, int32_t slice, io_service *io):
            m_max_num(max_num)
            , m_cycle(cycle)
            , m_flow_ctrl_timer(*io)
        {
            m_window = max_num / slice;
            m_cycle = m_cycle * 1000;
            m_cycle_slice = m_cycle / slice;

            if (0 == m_window || 0 == m_cycle_slice)
            {
                m_window = max_num;
                m_cycle_slice = m_cycle;
            }
            m_free_window = m_window;
            //start_flow_ctrl_timer();
        }

        bool flow_ctrl::over_speed(int32_t in_num)
        {
            if (m_free_window <= 0)
            {
                return true;
            }

            m_free_window -= in_num;
            if (m_free_window < 0)
            {
                return true;
            }
            return false;
        }
        void flow_ctrl::on_flow_ctrl_timer_expired(const boost::system::error_code& error)
        {
            if (boost::asio::error::operation_aborted == error)
            {
                LOG_DEBUG << "timer matrix manager timer error: aborted";
                return;
            }
//            LOG_DEBUG << "m_free_window: " << m_free_window;
            m_free_window = m_window + m_free_window;
            if (m_free_window > 2* m_max_num)
            {
                m_free_window = 2 * m_max_num;
            }

            start_flow_ctrl_timer();
        }

        void flow_ctrl::start()
        {
            start_flow_ctrl_timer();
        }

        void flow_ctrl::stop()
        {
            boost::system::error_code error;

            m_flow_ctrl_timer.cancel(error);
            if (error)
            {
                LOG_ERROR << "flow_ctrl cancel timer error: " << error;
            }
            else
            {
                LOG_DEBUG << "flow_ctrl stop timer";
            }
        }
        
        void flow_ctrl::start_flow_ctrl_timer()
        {
            try
            {
                m_flow_ctrl_timer.expires_from_now(std::chrono::milliseconds(m_cycle_slice));
                m_flow_ctrl_timer.async_wait(boost::bind(&flow_ctrl::on_flow_ctrl_timer_expired,
                    std::dynamic_pointer_cast<flow_ctrl>(shared_from_this()), boost::asio::placeholders::error));
                /*m_flow_ctrl_timer->async_wait(boost::bind(&flow_ctrl::on_flow_ctrl_timer_expired,
                    shared_from_this(), boost::asio::placeholders::error));*/
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "error" << e.what();
            }
        }
    }
}
