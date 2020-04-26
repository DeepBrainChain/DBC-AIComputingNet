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

#include <set>
#include "oss_client.h"
#include "db/ai_db_types.h"
#include <leveldb/db.h>
#include <chrono>
#include "rw_lock.h"
#include "image_manager.h"

//#include <boost/program_options/variables_map.hpp>
#include "service_module.h"
#include "container_worker.h"
#include "db/ai_provider_task_db.h"

#define CONTAINER_WORKER_IF m_container_worker->get_worer_if()

using namespace std;
using namespace matrix::core;
namespace bp = boost::process;


//used to interactive with remote system
namespace ai
{
    namespace dbc
    { 
        enum TASK_STATE
        {
            DBC_TASK_RUNNING,
            DBC_TASK_NULL,
            DBC_TASK_NOEXIST,
            DBC_TASK_STOPPED
        };

        struct task_time_stamp_comparator
        {
            bool operator() (const std::shared_ptr<ai_training_task> & t1, const std::shared_ptr<ai_training_task> & t2) const
            {
                return t1->received_time_stamp < t2->received_time_stamp;
            }
        };

        class task_scheduling
        {
        public:
            task_scheduling(std::shared_ptr<container_worker> m_container_worker);
            task_scheduling() = default;
            ~task_scheduling() = default;
            
            int32_t init_db(std::string db_name);
            virtual int32_t load_task() { return E_SUCCESS; }
            std::string get_gpu_spec(std::string s);
            int32_t change_gpu_id(std::shared_ptr<ai_training_task> task);
            int32_t update_task(std::shared_ptr<ai_training_task> task);
            vector<string> split(const string& str, const string& delim);
            int32_t update_task_commit_image(std::shared_ptr<ai_training_task> task);
            int32_t start_task(std::shared_ptr<ai_training_task> task);
            int32_t stop_task(std::shared_ptr<ai_training_task> task);
            int32_t start_task_from_new_image(std::shared_ptr<ai_training_task> task,std::string autodbcimage_version,std::string training_engine_new);
            int32_t create_task_from_image(std::shared_ptr<ai_training_task> task,std::string autodbcimage_version);
            TASK_STATE get_task_state(std::shared_ptr<ai_training_task> task);
            int32_t restart_task(std::shared_ptr<ai_training_task> task);
            int32_t stop_task_only_id(std::string task_id);
            std::string get_pull_log(std::string training_engine) { return m_pull_image_mng->get_out_log(training_engine); }
        protected:
            int32_t start_pull_image(std::shared_ptr<ai_training_task> task);
            int32_t stop_pull_image(std::shared_ptr<ai_training_task> task);
            int32_t delete_task(std::shared_ptr<ai_training_task> task);
        private:
            int32_t create_task(std::shared_ptr<ai_training_task> task);

        protected:
            ai_provider_task_db m_task_db;
            std::shared_ptr<image_manager> m_pull_image_mng = nullptr;
            std::shared_ptr<container_worker> m_container_worker = nullptr;
        };
    }
}
