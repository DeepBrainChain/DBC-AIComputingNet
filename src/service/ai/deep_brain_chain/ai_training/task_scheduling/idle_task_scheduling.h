/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   idle_task_scheduling.h
* description    :   idle_task_scheduling for definition
* date                  :   2018.10.18
* author            :   Regulus
**********************************************************************************/

#pragma once
#include "container_client.h"
#include <memory>
#include <string>
#include <boost/process.hpp>
#include <set>
#include "oss_client.h"
#include "ai_db_types.h"
#include <leveldb/db.h>
#include <chrono>
#include "nio_loop_group.h"
#include "rw_lock.h"
#include "image_manager.h"
#include "task_scheduling.h"
using namespace std;
using namespace matrix::core;

//used to interactive with remote system
namespace ai
{
    namespace dbc
    { 
        class idle_task_scheduling:public std::enable_shared_from_this<idle_task_scheduling>, task_scheduling
        {
            using fetch_task_handler = typename std::function<std::shared_ptr<idle_task_resp>()>;
        public:
            idle_task_scheduling(std::shared_ptr<container_worker> & container_worker_ptr);
            idle_task_scheduling() = default;
            ~idle_task_scheduling() = default;
            int32_t init();
            int32_t exec_task();
            int32_t stop_task();
            int32_t load_task();
            void update_idle_task();
            void set_task(std::shared_ptr<idle_task_resp> task);
            void set_fetch_handler(fetch_task_handler handler) { m_fetch_task_handler = handler; }
            int8_t task_status();

        private:
            std::shared_ptr<ai_training_task> m_idle_task = nullptr;
            //used to fetch idle task from oss. the handler ref oss fetch idle task func.
            fetch_task_handler m_fetch_task_handler;

            //if m_idle_state_begin=0, means dbc is not idle. 
            //if need to exec idle task, then dbc change the m_idle_state_begin to the current time,
            //then after m_max_idle_state_interval, dbc try to exec idle task.
            int64_t m_idle_state_begin = 0;    
            const int64_t  m_max_idle_state_interval = 3 * 60 * 1000; //3 min.180,000ms
        };
    }
}
