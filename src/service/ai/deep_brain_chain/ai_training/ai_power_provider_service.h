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
#include "bill_client.h"
#include <boost/process.hpp>
#include "image_manager.h"

using namespace matrix::core;


#define AI_TRAINING_TASK_TIMER                                      "training_task"
//#define AI_AUTH_TASK_TIMER                                          "auth_task"
//#define AI_PULLING_IMAGE_TIMER                                          "ai_pulling_image"
#define AI_TRAINING_TASK_TIMER_INTERVAL                 (30 * 1000)                                                 //30s timer
#define AI_PULLING_IMAGE_TIMER_INTERVAL                 (5*3600* 1000)                                                 //5h timer
#define AI_TRAINING_MAX_RETRY_TIMES                                  1
#define AI_TRAINING_MAX_TASK_COUNT                                    3

#define AI_TRAINING_TASK_SCRIPT_HOME                         "/home/dbc_utils/"
#define AI_TRAINING_TASK_SCRIPT                                       "dbc_task.sh"                                            //training shell script name
#define AI_TRAINING_BIND_LOCALTIME                                    "/etc/localtime:/etc/localtime:ro"                          //set docker container time=local host time
#define AI_TRAINING_BIND_TIMEZONE                                    "/etc/timezone:/etc/timezone:ro"                          //set docker container time=local host time

//gpu env
#define AI_TRAINING_ENV_PATH                                            "PATH=/usr/local/nvidia/bin:/usr/local/cuda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define AI_TRAINING_LD_LIBRARY_PATH                            "LD_LIBRARY_PATH=/usr/local/cuda/extras/CUPTI/lib64:/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
#define AI_TRAINING_NVIDIA_VISIBLE_DEVICES              "NVIDIA_VISIBLE_DEVICES=all"
#define AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES     "NVIDIA_DRIVER_CAPABILITIES=compute,utility"
#define AI_TRAINING_LIBRARY_PATH                                    "LIBRARY_PATH=/usr/local/cuda/lib64/stubs:"
#define AI_TRAINING_MOUNTS_SOURCE                              "/var/lib/nvidia-docker/volumes/nvidia_driver/384.111"
#define AI_TRAINING_MOUNTS_DESTINATION                  "/usr/local/nvidia"
#define AI_TRAINING_MOUNTS_MODE                                  "ro"
#define AI_TRAINING_CGROUP_PERMISSIONS                   "rwm"

#define DEFAULT_SPLIT_COUNT                                                         2
#define DEFAULT_NVIDIA_DOCKER_PORT                                                3476

const bool NEED_AUTH = true;

namespace image_rj = rapidjson;
namespace bp = boost::process;
namespace ai
{
	namespace dbc
	{

        enum container_status
        {
            container_unknown = 0,
            container_running,
            container_closed
        };

        enum nvidia_docker_version
        {
            NVIDIA_DOCKER_ONE = 1,
            NVIDIA_DOCKER_TWO = 2,
            NVIDIA_DOCKER_UNKNOWN = 0xff,
        };

        struct task_time_stamp_comparator
        {
            bool operator() (const std::shared_ptr<ai_training_task> & t1, const std::shared_ptr<ai_training_task> & t2) const
            {
                return t1->received_time_stamp < t2->received_time_stamp;
            }
        };

        class nvidia_config
        {
        public:

            std::string driver_name;

            std::string volume;

            std::list<std::string> devices;
        };

        class ai_power_provider_service : public service_module
        {
        public:

            ai_power_provider_service();

            virtual ~ai_power_provider_service() = default;

            virtual std::string module_name() const { return ai_power_provider_service_name; }

        protected:

            int32_t init_conf();

            void init_subscription();

            void init_invoker();

            void init_timer();

            int32_t init_db();

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

            std::shared_ptr<nvidia_config> get_nvidia_config_from_cli();

            std::shared_ptr<container_config> get_container_config(std::shared_ptr<ai_training_task> task);

            int32_t on_training_task_timer(std::shared_ptr<core_timer> timer);

            int32_t start_exec_training_task(std::shared_ptr<ai_training_task> task);

            int32_t check_training_task_status(std::shared_ptr<ai_training_task> task);

            int32_t write_task_to_db(std::shared_ptr<ai_training_task> task);

            int32_t load_task_from_db();

            int32_t load_container();
            int32_t load_bill_config();

            int32_t check_cpu_config(const double & cpu_info);
            int32_t check_memory_config(int64_t memory, int64_t memory_swap, int64_t shm_size);
            int32_t stop_task(std::shared_ptr<ai_training_task> task, training_task_status status);

            std::shared_ptr<auth_task_req> create_auth_task_req(std::shared_ptr<ai_training_task> task);
            int32_t auth_task(std::shared_ptr<ai_training_task> task);
            int32_t check_sign(const std::string message, const std::string &sign, const std::string &origin_id, const std::string & sign_algo);
            int32_t pull_image(std::shared_ptr<ai_training_task> task);
            int32_t check_pull_image(std::shared_ptr<ai_training_task> task);
            int32_t end_pull(std::shared_ptr<ai_training_task> task);
            int32_t check_pull_image_state(std::shared_ptr<ai_training_task> task);
            bool    task_need_auth(std::shared_ptr<ai_training_task> task);

            nvidia_docker_version get_nv_docker_version();

        protected:

            std::shared_ptr<leveldb::DB> m_prov_training_task_db;

            std::string m_container_ip;

            uint16_t m_container_port;

            std::shared_ptr<container_client> m_container_client;

            std::shared_ptr<container_client> m_nvidia_client;

            std::shared_ptr<bill_client> m_bill_client;

            std::unordered_map<std::string, std::shared_ptr<ai_training_task>> m_training_tasks;             //ai power provider cached training task in memory

            std::list<std::shared_ptr<ai_training_task>> m_queueing_tasks;

            uint32_t m_training_task_timer_id;
            //uint32_t m_auth_task_timer_id;

            std::shared_ptr<nvidia_config> m_nv_config;
            variables_map m_container_args;

            const int64_t m_nano_cpu = 1000000000;
            const int64_t m_g_bytes = 1073741824;

            //min disk_free 1024MB
            const uint32_t m_min_disk_free = 1024;
  
            std::string m_docker_root_dir = "";
            std::shared_ptr<image_manager> m_pull_image_mng = nullptr;
            bool m_auto_pull_image = true;
            int64_t m_auth_time_interval = 0;

            nvidia_docker_version m_nv_docker_version;
        };

    }

}



