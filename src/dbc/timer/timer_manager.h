#ifndef DBC_TIMER_MANAGER_H
#define DBC_TIMER_MANAGER_H

#include "util/utils.h"
#include "timer.h"
#include "network/protocol/service_message.h"

#define INVALID_TIMER_ID                        0
#define MAX_TIMER_ID                        0xFFFFFFFF

class service_module;

class timer_manager {
public:
    timer_manager(service_module *mdl);

    virtual ~timer_manager();

    //period: ms
    uint32_t add_timer(const std::string &name, uint32_t period, uint64_t repeat_times,
                       const std::string &session_id);

    void remove_timer(uint32_t timer_id);

    void on_time_point_notification(std::shared_ptr<dbc::network::message> msg);

    int32_t process(uint64_t time_tick);

protected:
    void remove_all_timers();

protected:
    service_module *m_module;

    std::list<std::shared_ptr<core_timer>> m_timer_queue;

    uint32_t m_timer_alloc_id;
};

#endif
