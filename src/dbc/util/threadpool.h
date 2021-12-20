#ifndef DBCPROJ_THREADPOOL_H
#define DBCPROJ_THREADPOOL_H

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>
#include <memory>
#include <utility>

class threadpool {
private:
    using ThreadTask = std::function<void()>;

    std::vector<std::thread> m_pool;
    std::queue<ThreadTask> m_threadtasks;
    std::mutex m_lock;
    std::condition_variable m_cv;
    std::atomic<bool> m_run{ true };

public:
    threadpool(int size = 10) {
        addThread(size);
    }

    virtual ~threadpool() {
        m_run = false;
        m_cv.notify_all();
        for (std::thread& thread : m_pool) {
            if(thread.joinable())
                thread.join();
        }
    }

    // 提交一个任务
    // 调用.get()获取返回值会等待任务执行完,获取返回值
    // 有两种方法可以实现调用类成员，
    // 一种是使用   bind： .commit(std::bind(&Dog::sayHello, &dog));
    // 一种是用   mem_fn： .commit(std::mem_fn(&Dog::sayHello), this)
    template<class F, class... Args>
    auto commit(F&& f, Args&&... args) ->std::future<decltype(f(args...))>
    {
        using RetType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<RetType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_threadtasks.emplace([task](){
                (*task)();
            });
        }

        m_cv.notify_one();
        return future;
    }

    void addThread(unsigned short size)
    {
        for (int i = 0; i < size; i++)
        {
            m_pool.emplace_back( [this] () {
                while (m_run) {
                    ThreadTask threadtask;
                    {
                        std::unique_lock<std::mutex> lock(m_lock);
                        m_cv.wait(lock, [this]{
                            return !m_run || !m_threadtasks.empty();
                        });

                        if (!m_run || m_threadtasks.empty())
                            return;

                        threadtask = move(m_threadtasks.front());
                        m_threadtasks.pop();
                    }
                    threadtask();
                }
            });
        }
    }
};

#endif //DBCPROJ_THREADPOOL_H
