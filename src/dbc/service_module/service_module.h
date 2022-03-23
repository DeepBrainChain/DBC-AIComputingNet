#ifndef DBC_SERVICE_MODULE_H
#define DBC_SERVICE_MODULE_H

#include "util/utils.h"
#include "../timer/timer.h"
#include "network/protocol/net_message.h"
#include "session.h"
#include "util/crypto/pubkey.h"
#include <mutex>

#define MAX_MSG_QUEUE_SIZE  102400
#define USE_SIGN_TIME       1548777600

using invoker_type = std::function<void(const std::shared_ptr<network::message> &msg)>;

using timer_invoker_type = std::function<void(const std::shared_ptr<core_timer>& timer)>;

class service_module
{
public:
    service_module() = default;

    virtual ~service_module();

    virtual ERRCODE init();

    virtual void exit();

	/** add timer & handle
     *  period: ms
     *  return: timer_id
    */
	uint32_t add_timer(const std::string &name, uint32_t delay, uint32_t period, uint64_t repeat_times,
		const std::string &session_id, const std::function<void(const std::shared_ptr<core_timer>&)>& handle = nullptr);

	void remove_timer(uint32_t timer_id);

    void remove_all_timer();

    /** register msg & handle */
    void reg_msg_handle(const std::string &msgname, const std::function<void(const std::shared_ptr<network::message>&)>& handle = nullptr);

    void unreg_msg_handle(const std::string &msgname);

    void remove_all_msg_handle();

    /** session */
	int32_t add_session(const std::string& session_id, const std::shared_ptr<service_session>& session);

	void remove_session(const std::string& session_id);

	std::shared_ptr<service_session> get_session(const std::string& session_id);

	int32_t get_session_count();

protected:
    virtual void init_timer() = 0;

    virtual void init_invoker() = 0;

private:
	void send(const std::shared_ptr<network::message>& msg);

    void thread_func();

	void on_msg_handle(std::shared_ptr<network::message>& msg);

	void on_timer_tick(uint64_t time_tick);

protected:
    bool m_running = false;

    std::queue<std::shared_ptr<network::message>> m_msg_queue;
    std::mutex m_msg_queue_mutex;
    std::condition_variable m_cond;
    std::thread* m_thread = nullptr;

    std::unordered_map<std::string, invoker_type> m_msg_invokers;

    uint32_t m_timer_alloc_id = 0;
	std::list<std::shared_ptr<core_timer>> m_timer_queue;
    std::unordered_map<std::string, timer_invoker_type> m_timer_invokers;

	RwMutex m_session_lock;
	std::unordered_map<std::string, std::shared_ptr<service_session>> m_sessions;
};

#endif
