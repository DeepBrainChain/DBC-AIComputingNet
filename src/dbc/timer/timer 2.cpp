#include "timer.h"
#include "timer_matrix_manager.h"

core_timer::core_timer(const std::string &name, uint32_t period, uint64_t repeat_times, const std::string & session_id)
    : m_name(name)
    , m_period(period)
    , m_repeat_times(repeat_times)
    , m_timer_id(0)
    , m_session_id(session_id)
{
    init();
}

void core_timer::init()
{
    assert(m_repeat_times > 0);
    m_period_as_tick = (uint32_t)std::round(m_period * 1.0 / DEFAULT_TIMER_INTERVAL);
    m_time_out_tick = timer_matrix_manager::get_cur_tick();

    cal_time_out_tick();
}

const std::string & core_timer::get_name() const
{
    return m_name;
}

uint32_t core_timer::get_timer_id() const
{
    return m_timer_id;
}

void core_timer::set_timer_id(uint32_t timer_id)
{
    m_timer_id = timer_id;
}

uint64_t core_timer::get_time_out_tick() const
{
    return m_time_out_tick;
}

void core_timer::cal_time_out_tick()
{
    m_time_out_tick += m_period_as_tick;
}
