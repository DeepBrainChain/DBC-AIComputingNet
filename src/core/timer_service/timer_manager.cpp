/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer_manager.cpp
* description    ：matrix timer manager
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/
#include "timer_manager.h"
#ifdef __RTX
#include "os_time.h"
#endif

namespace matrix
{
    namespace core
    {

        timer_manager::timer_manager(module *mdl)
            : m_module(mdl)
            , m_timer_alloc_id(0)
        {
            assert(m_module != nullptr);
        }

        timer_manager::~timer_manager()
        {
            remove_all_timers();
        }

        uint32_t timer_manager::add_timer(uint32_t name, uint32_t period, uint32_t delay)
        {
            std::shared_ptr<core_timer> timer(new core_timer(name, period, delay));
            timer->set_timer_id(m_timer_alloc_id++);
            m_timer_queue.push_back(timer);

            return timer->get_timer_id();
        }

        void timer_manager::remove_timer(uint32_t timer_id)
        {
            for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++)
            {
                auto found = it;
                std::shared_ptr<core_timer> timer = *it;
                if (timer_id == timer->get_timer_id())
                {
                    m_timer_queue.erase(found);
                }
            }
        }

        void timer_manager::process()
        {
            auto now = std::chrono::steady_clock::now();
            for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++)
            {
                std::shared_ptr<core_timer> timer = *it;
                if (timer->get_time_out_tick() < now)
                {
                    m_module->on_time_out(timer);
                    timer->cal_time_out_tick();
                }
            }
        }

        void timer_manager::remove_all_timers()
        {
            m_timer_queue.clear();
        }

    }

}



