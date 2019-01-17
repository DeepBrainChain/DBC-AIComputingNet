/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ai_power_provider_service.h
* description    :   ai_power_provider_service
* date                  :   2018.04.05
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include <leveldb/db.h>
#include <string>
#include "service_module.h"
#include "ai_db_types.h"
#include "container_client.h"
#include "task_common_def.h"
#include "document.h"
#include "oss_client.h"
#include <boost/process.hpp>
#include "image_manager.h"
#include "oss_task_manager.h"
#include "idle_task_scheduling.h"
#include "user_task_scheduling.h"
#include "container_worker.h"

#define AI_TRAINING_TASK_TIMER                                      "training_task"
//#define AI_TRAINING_TASK_TIMER_INTERVAL                             (30 * 1000)                                                 //30s timer
#define AI_PRUNE_TASK_TIMER                                      "prune_task"
#define AI_PRUNE_TASK_TIMER_INTERVAL                             (10*60*1000)                                                 //10min timer

using namespace matrix::core;

namespace image_rj = rapidjson;
namespace bp = boost::process;
namespace ai
{
	namespace dbc
	{
        class ai_power_provider_service : public service_module
        {
        public:
            ai_power_provider_service();

            virtual ~ai_power_provider_service() = default;

            virtual std::string module_name() const { return ai_power_provider_service_name; }

        protected:
            void init_subscription();

            void init_invoker();

            void init_timer();

            int32_t service_init(bpo::variables_map &options);

            int32_t service_exit();

        protected:
            int32_t on_start_training_req(std::shared_ptr<message> &msg);

            int32_t on_stop_training_req(std::shared_ptr<message> &msg);

            int32_t on_list_training_req(std::shared_ptr<message> &msg);

            int32_t on_logs_req(const std::shared_ptr<message> &msg);

            std::string format_logs(const std::string &raw_logs, uint16_t max_lines);

            int32_t on_get_task_queue_size_req(std::shared_ptr<message> &msg);

        protected:
            //ai power provider service
            int32_t on_training_task_timer(std::shared_ptr<core_timer> timer);
            int32_t on_prune_task_timer(std::shared_ptr<core_timer> timer);

            int32_t check_sign(const std::string message, const std::string &sign, const std::string &origin_id, const std::string & sign_algo);


            int32_t task_restart(std::shared_ptr<matrix::service_core::start_training_req> req);
            int32_t node_reboot(std::shared_ptr<matrix::service_core::start_training_req> req);
            int32_t task_start(std::shared_ptr<matrix::service_core::start_training_req> req);
       
        protected:
            uint32_t m_training_task_timer_id;
            uint32_t m_prune_task_timer_id;
            /////////////allow dbc to exec idle task, when dbc is not running ai user's task////////////////////////
            std::shared_ptr<oss_task_manager> m_oss_task_mng = nullptr;
            std::shared_ptr<idle_task_scheduling> m_idle_task_ptr = nullptr;

            std::shared_ptr<user_task_scheduling> m_user_task_ptr = nullptr;
            std::shared_ptr<container_worker> m_container_worker = nullptr;

            // urgent task
            std::shared_ptr<ai_training_task> m_urgent_task = nullptr;
        };

    }

}
