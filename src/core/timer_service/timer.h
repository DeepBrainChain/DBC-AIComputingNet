/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer.h
* description    ：matrix timer
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _BASE_TIME_H_
#define _BASE_TIME_H_

#include <chrono>
#include "common.h"

namespace matrix
{
    namespace core
    {
        class core_timer
        {
        public:

            //unit: second
            core_timer(uint32_t name, uint32_t period, uint32_t delay = 0);

            void init();

            uint32_t get_name();

            //timer ID
            uint32_t get_timer_id();

            void set_timer_id(uint32_t timer_id);

            //time out tick
#ifdef __RTX
            uint32_t get_time_out_tick();
#else
            std::chrono::time_point<std::chrono::steady_clock> get_time_out_tick();
#endif

            void cal_time_out_tick();

            //virtual void on_time_out() = 0;    

        protected:

            uint32_t m_name;

            bool m_is_first_start;                      //is first start

            uint32_t m_delay;                             //delay first time

            uint32_t m_period;                          //timer period

            uint32_t m_timer_id;                        //timer id

#ifdef __RTX
            uint64_t m_time_out_tick;               //超时tick, 1 ms / 1 tick
#else
            std::chrono::time_point<std::chrono::steady_clock> m_time_out_tick;
#endif

        };

    }

}


#endif


