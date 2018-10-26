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
#include "idle_task_scheduling.h"
using namespace std;
using namespace matrix::core;

//used to interactive with remote system
namespace ai
{
    namespace dbc
    { 
        class oss_task_manager:public std::enable_shared_from_this<oss_task_manager>, boost::noncopyable
        {
        public:
            oss_task_manager();
            ~oss_task_manager() = default;

            //void set_idle_task(std::shared_ptr<idle_task_scheduling> idle_task_ptr) { m_idle_task_ptr = idle_task_ptr; }
            
            int32_t init();
            int32_t load_oss_config();
            
            //send  auth_ai_user_task to oss system.             
            void reset_auth_interval() { m_auth_time_interval = 0;}            
            int32_t auth_task(std::shared_ptr<ai_training_task> task);
            bool task_need_auth(std::shared_ptr<ai_training_task> task);
            std::shared_ptr<auth_task_req> create_auth_task_req(std::shared_ptr<ai_training_task> task);
            bool can_exec_idle_task() { return m_enable_idle_task; }
            std::shared_ptr<idle_task_resp> fetch_idle_task();
            
        private:
            std::shared_ptr<oss_client> m_oss_client = nullptr;

        private:
            //next auth interval, depends on the oss's retern value 
            int64_t m_auth_time_interval = 0;

            //m_enable_billing=true, will send req contract message to oss.
            bool m_enable_billing = true;
        
        private:
            int64_t m_next_update_interval = 0;
            
            //try update idle task cycle.
            int64_t  m_get_idle_task_cycle;
            //m_enable_idle_task=true, will fetch idle task from oss
            bool m_enable_idle_task = true;
            const int32_t UPDATE_IDLE_TASK_MIN_CYCLE = 10 * 60 * 1000;   //10min
        };
    }
}
