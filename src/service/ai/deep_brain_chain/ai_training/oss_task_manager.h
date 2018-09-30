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
            IDLE_TASK_STOPPED
        };

        class oss_task_manager:public std::enable_shared_from_this<oss_task_manager>, boost::noncopyable
        {
            using get_idle_task_container_config = typename std::function<std::shared_ptr<container_config>(shared_ptr<ai_training_task>)>;

        public:
            oss_task_manager(std::shared_ptr<container_client> &container_client_ptr);
            oss_task_manager() = default;
            ~oss_task_manager() = default;
            
            int32_t init();
            void stop();

            int32_t load_oss_config();
            int32_t exec_idle_task(get_idle_task_container_config get_config);
            int32_t stop_idle_task();
            idle_task_state get_idle_task_state();

            //send  auth_ai_user_task to oss system. 
            std::shared_ptr<auth_task_req> create_auth_task_req(std::shared_ptr<ai_training_task> task);
            void reset_auth_interval() { m_auth_time_interval = 0;}
            int32_t auth_task(std::shared_ptr<ai_training_task> task);
            bool task_need_auth(std::shared_ptr<ai_training_task> task);
        private:
            int32_t init_db();
            int32_t load_idle_task();
            int32_t delete_idle_task();
            int32_t write_task_to_db();
            int32_t create_idle_task(get_idle_task_container_config get_config);
            int32_t get_idle_task();
            int32_t on_get_idle_task(const boost::system::error_code& error);
            int32_t start_get_idle_task_timer(int32_t interval);
            void stop_idle_task_timer();
        private:
            std::shared_ptr<oss_client> m_oss_client = nullptr;

        private:
            int64_t m_auth_time_interval = 0;
            bool m_allow_auth_task = true;
        
        private:
            std::shared_ptr<nio_loop_group> m_oss_timer_group = nullptr;
            std::shared_ptr<steady_timer> m_get_idle_task_timer = nullptr;
            std::shared_ptr<ai_training_task> m_idle_task = nullptr;
         
            std::shared_ptr<leveldb::DB> m_idle_task_db = nullptr;
            container_config m_idle_task_container_config;
            std::shared_ptr<container_client> m_container_client = nullptr;
            rw_lock m_lock_idle_task;

            //if m_idle_state_begin=0, means dbc is not idle. 
            //if need to exec idle task, then dbc change the m_idle_state_begin to the current time,
            //then after m_max_idle_state_interval, dbc try to exec idle task.
            int64_t m_idle_state_begin = 0;    
            const int32_t  m_max_idle_state_interval = 3 * 60 * 1000; //3 min.180,000ms
            
            //try fetch idle task, after dbc stated 10s.
            const int32_t  m_boot_get_idle_task_interval = 10;
            //when get idle task failed from oss, try again after m_get_idle_task_interval
            const int32_t  m_get_idle_task_interval = 5 * 60;
            //try update idle task by day.
            const int32_t  m_get_idle_task_day_interval = 24 *60 * 60; //24h
            bool m_allow_idle_task = true;
        };
    }
}
