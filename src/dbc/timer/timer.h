#ifndef DBC_TIMER_H
#define DBC_TIMER_H

#include "util/utils.h"
#include "timer_def.h"

class core_timer
{
public:
    //period: ms
    core_timer(const std::string& name, uint32_t delay, uint32_t period, uint64_t repeat_times,
        const std::string& session_id);

    std::string get_name() const {
        return m_name;
    }

    void set_timer_id(uint32_t timer_id) {
        m_timer_id = timer_id;
    }

    uint32_t get_timer_id() const {
        return m_timer_id;
    }

    uint64_t get_time_out_tick() const {
        return m_time_out_tick;
    }

    void cal_time_out_tick();

    void desc_repeat_times();

    uint64_t get_repeat_times() const { return m_repeat_times; }

	void set_session_id(const std::string &session_id) { m_session_id = session_id; }

    std::string get_session_id() { return m_session_id; }

private:
    std::string m_name;
    uint32_t m_delay = 0;  //unit: ms
    uint32_t m_period = 1;   //unit: ms
    uint64_t m_repeat_times = 1;
    uint32_t m_timer_id = INVALID_TIMER_ID;
    uint64_t m_time_out_tick = DEFAULT_TIMER_INTERVAL;    //unit: ms
    uint32_t m_period_as_tick;
    std::string m_session_id;
};

#endif
