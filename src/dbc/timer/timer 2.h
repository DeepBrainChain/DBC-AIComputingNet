#ifndef DBC_TIMER_H
#define DBC_TIMER_H

#include <string>
#include <chrono>
#include "util/utils.h"

#define ONLY_ONE_TIME       1

class core_timer
{
public:
    //unit: ms
    core_timer(const std::string &name, uint32_t period, uint64_t repeat_times, const std::string & session_id);

    void init();

    const std::string &get_name() const;

    uint32_t get_timer_id() const;

    void set_timer_id(uint32_t timer_id);

    //time out point

    uint64_t get_time_out_tick() const;

    void cal_time_out_tick();

    void desc_repeat_times() { if (m_repeat_times > 0) m_repeat_times--; }

    uint64_t get_repeat_times() const { return m_repeat_times; }

    std::string get_session_id() { return m_session_id; }

    void set_session_id(std::string session_id) { m_session_id = session_id; }

protected:

    std::string m_name;

    uint32_t m_period;                          //timer period: ms

    uint64_t m_repeat_times;

    uint32_t m_timer_id;                        //timer id

    uint64_t m_time_out_tick;                   //DEFAULT_TIMER_INTERVAL ms / 1 tick

    uint32_t m_period_as_tick;

    std::string m_session_id;
};

#endif
