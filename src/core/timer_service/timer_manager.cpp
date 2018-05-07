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
#include "time_tick_notification.h"
#include "timer_matrix_manager.h"
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
            assert(nullptr != m_module);
        }

        timer_manager::~timer_manager()
        {
            remove_all_timers();
        }

        uint32_t timer_manager::add_timer(const std::string &name, uint32_t period, uint64_t repeat_times, const std::string & session_id)
        {
            if (repeat_times < 1 || period < DEFAULT_TIMER_INTERVAL)
            {
                LOG_ERROR << "timer manager add timer error: repeat times or period";
                return E_DEFAULT;
            }

            std::shared_ptr<core_timer> timer(new core_timer(name, period, repeat_times, session_id));
            timer->set_timer_id(++m_timer_alloc_id);                //timer id begins from 1, 0 is invalid timer id
            m_timer_queue.push_back(timer);

            return timer->get_timer_id();
        }

        void timer_manager::remove_timer(uint32_t timer_id)
        {
            for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++)
            {
                std::shared_ptr<core_timer> timer = *it;
                if (timer_id == timer->get_timer_id())
                {
                    m_timer_queue.erase(it);
                    break;
                }
            }
        }

        void timer_manager::on_time_point_notification(std::shared_ptr<message> msg)
        {
            std::shared_ptr<time_tick_notification> notification = std::dynamic_pointer_cast<time_tick_notification>(msg->get_content());
            assert(nullptr != notification);

            this->process(notification->time_tick);
        }

        int32_t timer_manager::process(uint64_t time_tick)
        {
            std::shared_ptr<core_timer> timer;
            std::list<uint32_t> remove_timers_list;

            for (auto it = m_timer_queue.begin(); it != m_timer_queue.end(); it++)
            {
                timer  = *it;

                if (timer->get_time_out_tick() <= time_tick)
                {
                    //module callback
                    m_module->on_time_out(timer);

                    assert(timer->get_repeat_times() > 0);

                    //minus repeat times
                    timer->desc_repeat_times();
                    if (0 == timer->get_repeat_times())
                    {
                        remove_timers_list.push_back(timer->get_timer_id());
                    }
                    else
                    {
                        timer->cal_time_out_tick();                            //update next time out point
                    }
                }
            }

            //remove timers
            for (auto it : remove_timers_list)
            {
                remove_timer(it);                    //remove timer life period
            }

            remove_timers_list.clear();

            return E_SUCCESS;
        }

        void timer_manager::remove_all_timers()
        {
            m_timer_queue.clear();
        }

    }

}



