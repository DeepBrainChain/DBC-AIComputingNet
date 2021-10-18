#ifndef DBC_SERVICE_MODULE_H
#define DBC_SERVICE_MODULE_H

#include "util/utils.h"
#include "../timer/timer.h"
#include "../timer/timer_manager.h"
#include "network/protocol/service_message.h"
#include "session.h"
#include "util/crypto/pubkey.h"
#include <mutex>

#define MAX_MSG_QUEUE_SIZE               102400    //default message count

#define BIND_MESSAGE_INVOKER(MSG_NAME, FUNC_PTR)              invoker = std::bind(FUNC_PTR, this, std::placeholders::_1); m_invokers.insert({ MSG_NAME,{ invoker } });
#define SUBSCRIBE_BUS_MESSAGE(MSG_NAME)                       topic_manager::instance().subscribe(MSG_NAME, [this](std::shared_ptr<dbc::network::message> &msg) {return send(msg);});
#define USE_SIGN_TIME 1548777600

using invoker_type = typename std::function<void(const std::shared_ptr<dbc::network::message> &msg)>;
using timer_invoker_type = typename std::function<void(const std::shared_ptr<core_timer>& timer)>;

class service_module
{
public:
    typedef std::queue<std::shared_ptr<dbc::network::message>> queue_type;

    friend class timer_manager;

    service_module();

    virtual ~service_module() = default;

    virtual int32_t init();

    void start();

    void stop();

protected:
    virtual void init_timer() = 0;

    virtual void init_invoker() = 0;

    virtual void init_subscription() = 0;

    void init_time_tick_subscription();

    void send(const std::shared_ptr<dbc::network::message>& msg);

    void thread_func();

    void on_invoke(std::shared_ptr<dbc::network::message> &msg);

    void on_time_out(std::shared_ptr<core_timer> timer);

    uint32_t add_timer(std::string name, uint32_t period, uint64_t repeat_times, const std::string & session_id);                         //period, unit: ms

    void remove_timer(uint32_t timer_id);

    int32_t add_session(const std::string& session_id, const std::shared_ptr<service_session>& session);

    void remove_session(const std::string& session_id);

    std::shared_ptr<service_session> get_session(const std::string& session_id);

    int32_t get_session_count();

protected:
    bool m_running = false;

    queue_type m_msg_queue;
    std::mutex m_msg_queue_mutex;
    std::condition_variable m_cond;
    std::thread* m_thread = nullptr;

    std::shared_ptr<timer_manager> m_timer_manager;

    std::unordered_map<std::string, std::shared_ptr<service_session>> m_sessions;
    RwMutex m_session_lock;

    std::unordered_map<std::string, invoker_type> m_invokers;
    std::unordered_map<std::string, timer_invoker_type> m_timer_invokers;
};

#endif
