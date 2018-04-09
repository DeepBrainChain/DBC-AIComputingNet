/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer_manager.h
* description    ：matrix timer manager
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#pragma once


#include <list>
#include "timer.h"
#include "common.h"
#include "module.h"
#include "service_message.h"


#define INVALID_TIMER_ID                        0


namespace matrix
{
    namespace core
    {

        class timer_manager
        {
        public:

            timer_manager(module *mdl);

            virtual ~timer_manager();

            uint32_t add_timer(const std::string &name, uint32_t period, uint64_t repeat_times);                //period, unit: ms

            void remove_timer(uint32_t timer_id);

            void on_time_point_notification(std::shared_ptr<message> msg);

            int32_t process(uint64_t time_tick);

        protected:

            void remove_all_timers();

        protected:

            module * m_module;

            std::list<std::shared_ptr<core_timer>>  m_timer_queue;

            uint32_t m_timer_alloc_id;

        };

    }

}



