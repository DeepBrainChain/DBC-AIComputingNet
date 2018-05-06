/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer.h
* description    ：matrix timer
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#pragma once

#include <string>
#include <chrono>
#include "common.h"


#define ONLY_ONE_TIME                                               1


namespace matrix
{
    namespace core
    {
        class core_timer
        {
        public:

            //unit: ms
            core_timer(const std::string &name, uint32_t period, uint64_t repeat_times, const std::string & session_id);

            void init();

            const std::string &get_name() const;

            uint32_t get_timer_id() const;

            void set_timer_id(uint32_t timer_id);

            //time out point
#ifdef __RTX
            uint32_t get_time_out_tick();
#else
            uint64_t get_time_out_tick() const;
#endif

            void cal_time_out_tick();

            void desc_repeat_times() { if (m_repeat_times > 0) m_repeat_times--; }

            uint64_t get_repeat_times() const { return m_repeat_times; }

            std::string get_session_id() { return m_session_id; }

            void set_session_id(std::string session_id) { m_session_id = session_id; }

        protected:

            std::string m_name;

            uint32_t m_period;                          //timer period: ms

            uint64_t m_repeat_times;

            uint32_t m_timer_id;                        //timer id

#ifdef __RTX
            uint32_t m_time_out_tick;                   //超时tick, 1 ms / 1 tick
#else
            uint64_t m_time_out_tick;                   //DEFAULT_TIMER_INTERVAL ms / 1 tick
#endif

            uint32_t m_period_as_tick;

            std::string m_session_id;

        };

    }

}


