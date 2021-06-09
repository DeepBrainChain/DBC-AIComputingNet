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
#include "db/ai_db_types.h"
#include <leveldb/db.h>
#include <chrono>
#include "nio_loop_group.h"
#include "rw_lock.h"
#include "image_manager.h"
#include "task_scheduling.h"
#include "oss_task_manager.h"
#include "gpu_pool.h"
#include "server_initiator.h"

using namespace std;
using namespace matrix::core;

#define MAX_TASK_COUNT 200000
#define MAX_PRUNE_TASK_COUNT 160000
extern std::map<std::string, std::shared_ptr<ai::dbc::ai_training_task> > m_running_tasks;

namespace ai {
    namespace dbc {
        class user_task_scheduling : public task_scheduling {
            using auth_task_handler = typename std::function<int32_t(shared_ptr<ai_training_task>)>;
            using stop_idle_task_handler = typename std::function<int32_t()>;
        public:
            user_task_scheduling(std::shared_ptr<container_worker> container_worker_ptr,
                                 std::shared_ptr<vm_worker> vm_worker_ptr);

            user_task_scheduling() = default;

            ~user_task_scheduling() override = default;

            int32_t init(bpo::variables_map &options);



            int32_t exec_task();

            int32_t check_pull_image_state();

            int32_t check_training_task_status(std::shared_ptr<ai_training_task> task);

            //check the task is cached or not.
            //bool have_task(std::string task_id);
            int32_t add_task(std::shared_ptr<ai_training_task> task);

            int32_t add_update_task(std::shared_ptr<ai_training_task> task);

            std::shared_ptr<ai_training_task> find_task(std::string task_id);

            size_t get_user_cur_task_size() { return m_queueing_tasks.size() + m_running_tasks.size(); }

            size_t get_total_user_task_size() { return m_training_tasks.size(); }

            int32_t stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status, bool is_docker);

            void set_stop_idle_task_handler(stop_idle_task_handler handler) { m_stop_idle_task_handler = handler; }

            int32_t process_task();

            int32_t process_urgent_task(std::shared_ptr<ai_training_task> task);

            int32_t prune_task();

            std::string get_gpu_state();

            void update_gpu_info_from_proc();

            std::string get_active_tasks();

        private:
            int32_t load_task() override;

            int32_t prune_task(int16_t interval);

        private:
            bool m_is_computing_node = false;

            stop_idle_task_handler m_stop_idle_task_handler;

            std::unordered_map<std::string, std::shared_ptr<ai_training_task>> m_training_tasks;
            std::list<std::shared_ptr<ai_training_task>> m_queueing_tasks;

            int16_t m_prune_intervel = 0;
            gpu_pool m_gpu_pool;
        };
    }
}
