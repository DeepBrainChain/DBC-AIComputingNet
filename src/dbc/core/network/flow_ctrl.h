#ifndef DBC_NETWORK_FLOW_CTRL_H
#define DBC_NETWORK_FLOW_CTRL_H

#include "common.h"
#include "nio_loop_group.h"

namespace dbc
{
    namespace network
    {
        class flow_ctrl: public std::enable_shared_from_this<flow_ctrl>, public boost::noncopyable
        {
        public:
            flow_ctrl() = default;
            ~flow_ctrl() = default;
            bool over_speed(int32_t in_num);
            flow_ctrl(int32_t max_num, int32_t cycle, int32_t slice, io_service *io);
            void start();
            void stop();
        private:
            void start_flow_ctrl_timer();
            void on_flow_ctrl_timer_expired(const boost::system::error_code& error);
        private:
            int32_t m_max_num;
            int32_t m_cycle;
            int32_t m_window;
            int32_t m_cycle_slice;
            std::atomic<int32_t> m_free_window;
        private:
            steady_timer m_flow_ctrl_timer;
            //std::shared_ptr<steady_timer> m_flow_ctrl_timer;
        };
    }
}

#endif
