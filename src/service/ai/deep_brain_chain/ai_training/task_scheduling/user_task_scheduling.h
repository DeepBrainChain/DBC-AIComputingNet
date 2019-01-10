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
#include "image_manager.h"
#include "task_scheduling.h"
#include "oss_task_manager.h"
using namespace std;
using namespace matrix::core;

#define MAX_TASK_COUNT 200000
#define MAX_PRUNE_TASK_COUNT 160000

//used to interactive with remote system
namespace ai
{
    namespace dbc
    { 
        class user_task_scheduling:public std::enable_shared_from_this<user_task_scheduling>, public task_scheduling
        {
            using auth_task_handler = typename std::function<int32_t(shared_ptr<ai_training_task>)>;
            using stop_idle_task_handler = typename std::function<int32_t()>;
        public:
            user_task_scheduling(std::shared_ptr<container_worker> &container_worker_ptr);
            user_task_scheduling() = default;
            ~user_task_scheduling() = default;
           
            int32_t init();
            int32_t load_task();
            int32_t exec_task();

            int32_t check_pull_image_state();
            int32_t check_training_task_status();

            //check the task is cached or not.
            //bool have_task(std::string task_id);
            void add_task(std::shared_ptr<ai_training_task> task);
            std::shared_ptr<ai_training_task> find_task(std::string task_id);

            size_t get_user_cur_task_size() {return m_queueing_tasks.size();}
            size_t get_total_user_task_size() { return m_training_tasks.size(); }

            int32_t stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status);

            void set_auth_handler(auth_task_handler handler) { m_auth_task_handler = handler; }
            void set_stop_idle_task_handler(stop_idle_task_handler handler) { m_stop_idle_task_handler = handler; }

            int32_t process_task();

            int32_t process_urgent_task(std::shared_ptr<ai_training_task> task);

            int32_t prune_task();
        private:
            int32_t auth_task(std::shared_ptr<ai_training_task> task);
            int32_t prune_task(int16_t interval);

        protected:
            std::list<std::shared_ptr<ai_training_task>> m_queueing_tasks;
        private:
            auth_task_handler m_auth_task_handler;
            stop_idle_task_handler m_stop_idle_task_handler;
            //std::shared_ptr<oss_task_manager> m_oss_task_mng = nullptr;
            std::unordered_map<std::string, std::shared_ptr<ai_training_task>> m_training_tasks;
            int16_t m_prune_intervel = 0;
        };
    }
}
