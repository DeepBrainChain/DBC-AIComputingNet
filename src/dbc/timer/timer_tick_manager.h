#ifndef DBC_TIMER_TICK_MANAGER_H
#define DBC_TIMER_TICK_MANAGER_H

#include "util/utils.h"
#include "timer_def.h"
#include "network/nio_loop_group.h"
#include "network/protocol/service_message.h"
#include "service_module/service_module.h"

class timer_tick_manager : public std::enable_shared_from_this<timer_tick_manager>
{
public:
    timer_tick_manager() = default;

    virtual ~timer_tick_manager() = default;

    ERRCODE init();

    void exit();

    static uint64_t get_cur_tick() { return m_cur_tick; }

protected:
    std::shared_ptr<dbc::network::message> make_time_tick_notification();

    void on_timer_expired(const boost::system::error_code& error);

private:
    std::shared_ptr<dbc::network::nio_loop_group> m_timer_group;
    std::shared_ptr<steady_timer> m_timer;
    static std::atomic<uint64_t> m_cur_tick;
};

#endif
