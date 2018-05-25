/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : callback_wait.h
* description       : wait for a condition and execute call back function
* date              : 2018.03.31
* author            : Bruce Feng
**********************************************************************************/

#pragma once


#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <boost/program_options/variables_map.hpp>


using namespace std::chrono;
using namespace boost::program_options;


using default_callback_functor_type = void();


namespace matrix
{
    namespace core
    {

        extern void default_dummy();

        template<typename function_type = default_callback_functor_type>
        class callback_wait
        {
        public:

            callback_wait() : m_flag(false), m_callback(default_dummy) { }

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

    }

}