/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：timer_manager.h
* description    ：matrix timer manager
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/

#ifndef _TIMER_MANAGER_H_
#define _TIMER_MANAGER_H_

#include <list>
#include "timer.h"
#include "common.h"
#include "module.h"

namespace matrix
{
    namespace core
    {
        #define DEFAULT_TIMER              0xFFFFFFFF

        class timer_manager
        {
        public:

            timer_manager(module *mdl);

            virtual ~timer_manager();

            uint32_t add_timer(uint32_t name, uint32_t period, uint32_t delay = 0);         //unit: second

            void remove_timer(uint32_t timer_id);

            void process();

        protected:

            void remove_all_timers();

        protected:

            module *m_module;

            std::list<std::shared_ptr<core_timer>>  m_timer_queue;

            uint32_t m_timer_alloc_id;

        };

    }

}


#endif


