#include "service_module.h"
#include "service_message_id.h"
#include "../timer/timer_tick_manager.h"
#include "../server/server.h"
#include "../timer/time_tick_notification.h"
#include <boost/format.hpp>

service_module::~service_module() {
	this->exit();
}

ERRCODE service_module::init()
{
    init_timer();

    init_invoker();

	topic_manager::instance().subscribe(TIMER_TICK_NOTIFICATION, [this](std::shared_ptr<dbc::network::message>& msg) {
		send(msg);
	});

	m_running = true;
	if (m_thread == nullptr) {
		m_thread = new std::thread(&service_module::thread_func, this);
	}

	return ERR_SUCCESS;
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

void service_module::exit()
{
    m_running = false;
    m_cond.notify_all();
    if (m_thread != nullptr && m_thread->joinable()) {
        m_thread->join();
    }
    delete m_thread;
    m_thread = nullptr;

	remove_all_timer();
	remove_all_msg_handle();
	RwMutex::WriteLock wlock(m_session_lock);
	m_sessions.clear();
}

void service_module::thread_func()
{
	std::queue<std::shared_ptr<dbc::network::message>> tmp_msg_queue;

    while (m_running)
    {
        {
            std::unique_lock<std::mutex> lock(m_msg_queue_mutex);
            m_cond.wait_for(lock, std::chrono::milliseconds(500), [this] { 
                return !m_running || !m_msg_queue.empty(); 
            });
            m_msg_queue.swap(tmp_msg_queue);
        }

        std::shared_ptr<dbc::network::message> msg;
        while (!tmp_msg_queue.empty())
        {
            msg = tmp_msg_queue.front();
            tmp_msg_queue.pop();
            on_msg_handle(msg);
        }
    }
}

void service_module::on_msg_handle(std::shared_ptr<dbc::network::message> &msg)
{
    std::string msg_name = msg->get_name();
    if (msg_name == TIMER_TICK_NOTIFICATION)
    {
        std::shared_ptr<time_tick_notification> content = std::dynamic_pointer_cast<time_tick_notification>(msg->get_content());
        this->on_timer_tick(content->time_tick);
    }
    else
    {
        auto it = m_msg_invokers.find(msg->get_name());
        if (it != m_msg_invokers.end()) {
            auto func = it->second;
            func(msg);
        }
    }
}

void service_module::on_timer_tick(uint64_t time_tick)
{
	std::shared_ptr<core_timer> timer;
	std::list<uint32_t> remove_timers_list;

	for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++)
	{
		timer = *it;

		if (timer->get_time_out_tick() <= time_tick)
		{
			auto it = m_timer_invokers.find(timer->get_name());
			if (it != m_timer_invokers.end()) {
				auto func = it->second;
				func(timer);
			}

			timer->desc_repeat_times();
			if (0 == timer->get_repeat_times())
			{
				remove_timers_list.push_back(timer->get_timer_id());
			}
			else
			{
				timer->cal_time_out_tick();
			}
		}
	}

	for (auto it : remove_timers_list)
	{
		remove_timer(it);
	}
}

uint32_t service_module::add_timer(const std::string &name, uint32_t period, uint64_t repeat_times, 
    const std::string &session_id, const std::function<void(const std::shared_ptr<core_timer>&)>& handle)
{
	if (repeat_times < 1 || period < DEFAULT_TIMER_INTERVAL) {
		LOG_ERROR << "repeat_times or period is invalid";
		return INVALID_TIMER_ID;
	}

	std::shared_ptr<core_timer> timer(new core_timer(name, period, repeat_times, session_id));
	if (MAX_TIMER_ID == m_timer_alloc_id) {
		LOG_ERROR << "can not allocate new timer id";
		return INVALID_TIMER_ID;
	}
	timer->set_timer_id(++m_timer_alloc_id);    //timer id begins from 1, 0 is invalid timer id
	m_timer_queue.push_back(timer);

	if (handle != nullptr) {
		m_timer_invokers[name] = handle;
	}

	return timer->get_timer_id();
}

void service_module::remove_timer(uint32_t timer_id)
{
	std::string name;
	for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++) {
		std::shared_ptr<core_timer> timer = *it;
		if (timer_id == timer->get_timer_id()) {
			name = (*it)->get_name();
			m_timer_queue.erase(it);
			break;
		}
	}

	if (!name.empty())
		m_timer_invokers.erase(name);
}

void service_module::remove_all_timer() {
	m_timer_queue.clear();
	m_timer_invokers.clear();
}

void service_module::reg_msg_handle(const std::string& msgname, 
	const std::function<void(const std::shared_ptr<dbc::network::message>&)>& handle /* = nullptr */) {
	if (handle != nullptr) {
		m_msg_invokers[msgname] = handle;
		topic_manager::instance().subscribe(msgname, [this](std::shared_ptr<dbc::network::message>& msg) {
			send(msg);
		});
	}
}

void service_module::unreg_msg_handle(const std::string& msgname) {
	m_msg_invokers.erase(msgname);
	topic_manager::instance().unsubscribe<void>(msgname);
}

void service_module::remove_all_msg_handle() {
	for (auto& it : m_msg_invokers) {
		topic_manager::instance().unsubscribe<void>(it.first);
	}
	m_msg_invokers.clear();
}

int32_t service_module::add_session(const std::string& session_id, const std::shared_ptr<service_session>& session)
{
    RwMutex::WriteLock wlock(m_session_lock);
    auto it = m_sessions.find(session_id);
    if (it == m_sessions.end()) {
        m_sessions.insert({ session_id, session });
        return ERR_SUCCESS;
    } else {
        return ERR_ERROR;
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
