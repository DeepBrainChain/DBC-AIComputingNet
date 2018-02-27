/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer.cpp
* description    ：matrix timer
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/
#include "timer.h"
#ifdef __RTX
#include "os_time.h"
#endif

namespace matrix
{
    namespace core
    {

        core_timer::core_timer(uint32_t name, uint32_t period, uint32_t delay) 
            : m_name(name)
            , m_is_first_start(true)
            , m_delay(delay)
            , m_period(period)
            , m_timer_id(0)
#ifdef __RTX
            , m_time_out_tick(0)
#endif
        {
            init();
        }

        void core_timer::init()
        {
            cal_time_out_tick();

            m_is_first_start = false;
        }

        uint32_t core_timer::get_name()
        {
            return m_name;
        }

        uint32_t core_timer::get_timer_id()
        {
            return m_timer_id;
        }

        void core_timer::set_timer_id(uint32_t timer_id)
        {
            m_timer_id = timer_id;
        }

#ifdef __RTX
        uint32_t core_timer::get_time_out_tick()
        {
            return m_time_out_tick;
        }

        void core_timer::cal_time_out_tick()
        {
            if (true == m_is_first_start && 0 != m_delay)
            {
                m_time_out_tick = OSTime::get_sys_tick_up_tme() + m_delay;
            }
            else
            {
                m_time_out_tick = OSTime::get_sys_tick_up_tme() + m_period;
            }
        }

#else
        std::chrono::time_point<std::chrono::steady_clock> core_timer::get_time_out_tick()
        {
            return m_time_out_tick;
        }

        void core_timer::cal_time_out_tick()
        {
            m_time_out_tick = std::chrono::steady_clock::now() 
                                        + ((m_is_first_start && m_delay) ? std::chrono::seconds(m_delay) : std::chrono::seconds(m_period));
        }
#endif



    }

}


