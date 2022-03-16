#ifndef DBC_SESSION_H
#define DBC_SESSION_H

#include "common/common.h"
#include "service_context.h"

using namespace boost::program_options;

class service_session
{
public:
    service_session() : m_timer_id(INVALID_TIMER_ID) {}

    service_session(uint32_t timer_id, std::string session_id) : m_timer_id(timer_id), m_session_id(session_id) {}

    virtual ~service_session() { clear(); }

    uint32_t get_timer_id() { return m_timer_id; }

    void set_timer_id(uint32_t timer_id) { m_timer_id = timer_id; }

    std::string get_session_id() { return m_session_id; }

    void set_session_id(std::string session_id) { m_session_id = session_id; }

    service_context & get_context() { return m_context; }

    void clear() { m_context.get_args().clear(); }

protected:
    uint32_t m_timer_id;

    std::string m_session_id;

    service_context m_context;
};

#endif
