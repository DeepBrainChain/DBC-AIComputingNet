#ifndef DBC_RW_LOCK_H
#define DBC_RW_LOCK_H

#include <mutex>
#include <condition_variable>

class rw_lock
{
public:
    rw_lock() : m_read_count(0), m_write_count(0), m_is_writing_status(false)
    {

    }

    ~rw_lock() = default;

    void read_lock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_read_cond.wait(lock, [=]()->bool {return 0 == m_write_count;});
        m_read_count++;
    }

    void read_unlock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if ((--m_read_count == 0) && (m_write_count > 0))
        {
            m_write_cond.notify_one();
        }
    }

    void write_lock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_write_count++;
        m_write_cond.wait(lock, [=]()->bool {return (0 == m_read_count) && (!m_is_writing_status);});
        m_is_writing_status = true;
    }

    void write_unlock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        (--m_write_count == 0) ? m_read_cond.notify_all() : m_write_cond.notify_one();
        m_is_writing_status = false;
    }

private:

    volatile uint32_t m_read_count;

    volatile uint32_t m_write_count;

    volatile bool m_is_writing_status;

    std::mutex m_mutex;

    std::condition_variable m_write_cond;

    std::condition_variable m_read_cond;

};

template<typename _lockable>
class write_lock_guard
{
public:

    write_lock_guard(_lockable &lock) : m_rw_lock(lock)
    {
        m_rw_lock.write_lock();
    }

    ~write_lock_guard()
    {
        m_rw_lock.write_unlock();
    }

    write_lock_guard() = delete;
    write_lock_guard(const write_lock_guard&) = delete;
    write_lock_guard& operator=(const write_lock_guard&) = delete;

private:

    _lockable & m_rw_lock;
};

template<typename _lockable>
class read_lock_guard
{
public:

    read_lock_guard(_lockable &lock) : m_rw_lock(lock)
    {
        m_rw_lock.read_lock();
    }

    ~read_lock_guard()
    {
        m_rw_lock.read_unlock();
    }

    read_lock_guard() = delete;
    read_lock_guard(const read_lock_guard&) = delete;
    read_lock_guard& operator=(const read_lock_guard&) = delete;

private:

    _lockable & m_rw_lock;
};

#endif
