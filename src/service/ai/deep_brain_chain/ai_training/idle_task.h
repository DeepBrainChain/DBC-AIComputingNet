/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   oss_task_manager.h
* description    :   oss_task_manager for definition
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
using namespace std;
using namespace matrix::core;

//used to interactive with remote system
namespace ai
{
    namespace dbc
    { 
        enum idle_task_state
        {
            IDLE_TASK_RUNNING,
            IDLE_TASK_NOEXIST,
            IDLE_TASK_STOPPED
        };

        class idle_task:public std::enable_shared_from_this<idle_task>, boost::noncopyable
        {
            using get_idle_task_container_config = typename std::function<std::shared_ptr<container_config>(shared_ptr<ai_training_task>)>;

        public:
            idle_task(std::shared_ptr<container_client> &container_client_ptr);
            idle_task() = default;
            ~idle_task() = default;
            int32_t init_db();
            int32_t exec_task(get_idle_task_container_config get_config);
            int32_t stop_task();
            idle_task_state get_task_state();
            void set_task(std::shared_ptr<idle_task_resp> task);
        private:
            int32_t create_task(get_idle_task_container_config get_config);
            int32_t load_task();
            int32_t write_task_to_db();
            int32_t delete_task();
            

        private:
            std::shared_ptr<ai_training_task> m_idle_task = nullptr;         
            std::shared_ptr<leveldb::DB> m_idle_task_db = nullptr;
            container_config m_idle_task_container_config;
            std::shared_ptr<container_client> m_container_client = nullptr;

            //if m_idle_state_begin=0, means dbc is not idle. 
            //if need to exec idle task, then dbc change the m_idle_state_begin to the current time,
            //then after m_max_idle_state_interval, dbc try to exec idle task.
            int64_t m_idle_state_begin = 0;    
            const int64_t  m_max_idle_state_interval = 3 * 60 * 1000; //3 min.180,000ms            
        };
    }
}
