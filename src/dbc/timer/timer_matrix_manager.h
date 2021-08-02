#ifndef DBC_TIMER_MATRIX_MANAGER_H
#define DBC_TIMER_MATRIX_MANAGER_H

#include "util/utils.h"
#include "../module/module.h"
#include "network/nio_loop_group.h"
#include "network/protocol/service_message.h"

#define DEFAULT_TIMER_MATRIX_THREAD_COUNT           1
#define DEFAULT_TIMER_INTERVAL                      100 //unit:ms for one tick

typedef std::atomic<std::uint64_t> tick_type;

class timer_matrix_manager : public module, public std::enable_shared_from_this<timer_matrix_manager>, boost::noncopyable
{
    typedef void (timer_handler_type)(const boost::system::error_code &);

public:
    timer_matrix_manager();

    ~timer_matrix_manager() = default;

    virtual std::string module_name() const { return "timer matrix manager"; };

    virtual int32_t init(bpo::variables_map &options);

    virtual int32_t start();

    virtual int32_t stop();

    virtual int32_t exit();

    static uint64_t get_cur_tick() { return m_cur_tick; }

protected:
    void start_timer();

    void stop_timer();

    std::shared_ptr<dbc::network::message> make_time_tick_notification();

    void on_timer_expired(const boost::system::error_code& error);

    std::shared_ptr<dbc::network::nio_loop_group> m_timer_group;

    //std::function<timer_handler_type> m_timer_handler;

    std::shared_ptr<steady_timer> m_timer;

    static tick_type m_cur_tick;
};

#endif
