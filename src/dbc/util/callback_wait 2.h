#ifndef DBC_CALLBACK_WAIT_H
#define DBC_CALLBACK_WAIT_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <boost/program_options/variables_map.hpp>

using namespace std::chrono;
using namespace boost::program_options;

template<typename function_type = void()>
class callback_wait
{
public:
    callback_wait() : m_flag(false), m_callback(nullptr) { }

    callback_wait(function_type functor) : m_flag(false), m_callback(functor) {}

    virtual ~callback_wait() = default;

    bool flag() const
    {
        return m_flag.load(std::memory_order_acquire);
    }

    void set()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_flag.store(true, std::memory_order_release);
        }

        m_cv.notify_all();
    }

    void reset()
    {
        m_flag.store(false, std::memory_order_release);
    }

    virtual bool wait_for(milliseconds time_ms)
    {
        bool ret = false;

        if (false == this->flag())
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            ret = m_cv.wait_for(lock, time_ms, [this]()->bool { return m_flag.load(std::memory_order_acquire); });
        }

        if (true == ret && nullptr != m_callback)
        {
            m_callback();
        }
        return ret;
    }

protected:
    std::mutex m_mutex;

    std::condition_variable m_cv;

    std::atomic<bool> m_flag;

    std::function<function_type> m_callback;
};

#endif
