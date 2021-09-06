#include "service_module.h"
#include "service_message_id.h"
#include "../timer/timer_matrix_manager.h"
#include "../server/server.h"
#include "../timer/time_tick_notification.h"
#include <boost/format.hpp>

service_module::service_module()
{
    m_timer_manager = std::make_shared<timer_manager>(this);
}

int32_t service_module::init(bpo::variables_map &options)
{
    init_timer();

    init_invoker();

    init_subscription();

    init_time_tick_subscription();

    return E_SUCCESS;
}

void service_module::init_time_tick_subscription() {
    topic_manager::instance().subscribe(TIMER_TICK_NOTIFICATION, [this](std::shared_ptr<dbc::network::message> &msg) {
        send(msg);
    });
}

void service_module::start()
{
    m_running = true;
    if (m_thread == nullptr) {
        m_thread = new std::thread(&service_module::thread_func, this);
    }
}

void service_module::stop()
{
    m_running = false;

    if (m_thread != nullptr && m_thread->joinable()) {
        m_thread->join();
    }

    delete m_thread;
}

void service_module::thread_func()
{
    queue_type tmp_msg_queue;

    while (m_running)
    {
        {
            std::unique_lock<std::mutex> lock(m_msg_queue_mutex);
            std::chrono::milliseconds ms(500);
            m_cond.wait_for(lock, ms, [this]()->bool { return !m_msg_queue.empty(); });
            if (m_msg_queue.empty())
                continue;
            m_msg_queue.swap(tmp_msg_queue);
        }

        std::shared_ptr<dbc::network::message> msg;
        while (!tmp_msg_queue.empty())
        {
            msg = tmp_msg_queue.front();
            tmp_msg_queue.pop();
            on_invoke(msg);
        }
    }
}

void service_module::on_invoke(std::shared_ptr<dbc::network::message> &msg)
{
    std::string msg_name = msg->get_name();
    if (msg_name == TIMER_TICK_NOTIFICATION)
    {
        std::shared_ptr<time_tick_notification> content = std::dynamic_pointer_cast<time_tick_notification>(msg->get_content());
        m_timer_manager->process(content->time_tick);
    }
    else
    {
        auto it = m_invokers.find(msg->get_name());
        if (it != m_invokers.end()) {
            auto func = it->second;
            func(msg);
        }
    }
}

void service_module::send(const std::shared_ptr<dbc::network::message>& msg)
{
    std::unique_lock<std::mutex> lock(m_msg_queue_mutex);
    if (m_msg_queue.size() < MAX_MSG_QUEUE_SIZE)
    {
        m_msg_queue.push(msg);
    }
    else
    {
        LOG_ERROR << "service module message queue is full";
        return;
    }

    if (!m_msg_queue.empty())
    {
        m_cond.notify_all();
    }
}

void service_module::on_time_out(std::shared_ptr<core_timer> timer)
{
    auto it = m_timer_invokers.find(timer->get_name());
    if (it != m_timer_invokers.end()) {
        auto func = it->second;
        func(timer);
    }
}

uint32_t service_module::add_timer(std::string name, uint32_t period, uint64_t repeat_times, const std::string & session_id)
{
    return m_timer_manager->add_timer(name, period, repeat_times, session_id);
}

void service_module::remove_timer(uint32_t timer_id)
{
    m_timer_manager->remove_timer(timer_id);
}

int32_t service_module::add_session(const std::string& session_id, const std::shared_ptr<service_session>& session)
{
    RwMutex::WriteLock wlock(m_session_lock);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        m_sessions.insert({ session_id, session });
        return E_SUCCESS;
    } else {
        return E_DEFAULT;
    }
}

void service_module::remove_session(const std::string& session_id)
{
    RwMutex::WriteLock wlock(m_session_lock);
    m_sessions.erase(session_id);
}

std::shared_ptr<service_session> service_module::get_session(const std::string& session_id)
{
    RwMutex::ReadLock rlock(m_session_lock);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

int32_t service_module::get_session_count() {
    RwMutex::ReadLock rlock(m_session_lock);
    return m_sessions.size();
}
