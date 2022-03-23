#ifndef DBC_NETWORK_FLOW_CTRL_H
#define DBC_NETWORK_FLOW_CTRL_H

#include "common/common.h"
#include "io_service_pool.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>

using namespace boost::asio;

namespace network
{
    class flow_ctrl: public std::enable_shared_from_this<flow_ctrl>, public boost::noncopyable
    {
    public:
        flow_ctrl() = default;

        virtual ~flow_ctrl() = default;

        bool over_speed(int32_t in_num);

        flow_ctrl(int32_t max_num, int32_t cycle, int32_t slice, io_service *ioservice);

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
        steady_timer m_flow_ctrl_timer;
    };
}

#endif
