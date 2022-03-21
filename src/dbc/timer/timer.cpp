#include "timer.h"
#include "timer_tick_manager.h"

core_timer::core_timer(const std::string &name, uint32_t delay, uint32_t period, uint64_t repeat_times,
                       const std::string &session_id)
    : m_name(name), m_period(period), m_repeat_times(repeat_times)
    , m_timer_id(0), m_session_id(session_id)
{
	m_period_as_tick = (uint32_t)std::round(m_period * 1.0 / DEFAULT_TIMER_INTERVAL);
	m_time_out_tick = timer_tick_manager::get_cur_tick() + (uint32_t)std::round(delay * 1.0 / DEFAULT_TIMER_INTERVAL);
}

void core_timer::cal_time_out_tick()
{
    m_time_out_tick += m_period_as_tick;
}

void core_timer::desc_repeat_times() {
    if (m_repeat_times > 0) 
        m_repeat_times -= 1;
}
