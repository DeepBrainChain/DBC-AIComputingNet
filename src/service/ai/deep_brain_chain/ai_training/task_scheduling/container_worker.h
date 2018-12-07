/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_worker.h
* description    :   container_worker for definition
* date                  :   2018.10.25
* author            :   Regulus
**********************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "http_client.h"
#include "container_message.h"
#include "ai_db_types.h"
#include "container_message.h"
#include "service_module.h"
#include "task_common_def.h"
#include "container_client.h"

using namespace std;
using namespace matrix::core;
#define DEFAULT_STOP_CONTAINER_TIME                                  2                                                          //stop time out

#define AI_TRAINING_TASK_TIMER                                      "training_task"
//#define AI_TRAINING_TASK_TIMER_INTERVAL                             (30 * 1000)                                                 //30s timer
#define AI_PULLING_IMAGE_TIMER_INTERVAL                             (5*3600* 1000)                                              //5h timer
#define AI_TRAINING_MAX_RETRY_TIMES                                 1
#define AI_TRAINING_MAX_TASK_COUNT                                  3

#define AI_TRAINING_TASK_SCRIPT_HOME                                "/home/dbc_utils/"
#define AI_TRAINING_TASK_SCRIPT                                     "dbc_task.sh"                                               //training shell script name
#define AI_TRAINING_BIND_LOCALTIME                                  "/etc/localtime:/etc/localtime:ro"                          //set docker container time=local host time
#define AI_TRAINING_BIND_TIMEZONE                                   "/etc/timezone:/etc/timezone:ro"                            //set docker container time=local host time

//gpu env
#define AI_TRAINING_ENV_PATH                                        "PATH=/usr/local/nvidia/bin:/usr/local/cuda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define AI_TRAINING_LD_LIBRARY_PATH                                 "LD_LIBRARY_PATH=/usr/local/cuda/extras/CUPTI/lib64:/usr/local/nvidia/lib:/usr/local/nvidia/lib64"
#define AI_TRAINING_NVIDIA_VISIBLE_DEVICES                          "NVIDIA_VISIBLE_DEVICES=all"
#define AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES                      "NVIDIA_DRIVER_CAPABILITIES=compute,utility"
#define AI_TRAINING_LIBRARY_PATH                                    "LIBRARY_PATH=/usr/local/cuda/lib64/stubs:"
#define AI_TRAINING_MOUNTS_SOURCE                                   "/var/lib/nvidia-docker/volumes/nvidia_driver/384.111"
#define AI_TRAINING_MOUNTS_DESTINATION                              "/usr/local/nvidia"
#define AI_TRAINING_MOUNTS_MODE                                     "ro"
#define AI_TRAINING_CGROUP_PERMISSIONS                              "rwm"

#define DEFAULT_SPLIT_COUNT                                         2
#define DEFAULT_NVIDIA_DOCKER_PORT                                  3476

namespace ai
{
    namespace dbc
    { 
        class nvidia_config
        {
        public:

            std::string driver_name;

            std::string volume;

            std::list<std::string> devices;
        };

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

        class container_worker
        {
        public:

            container_worker();

            ~container_worker() = default;
        public:
            int32_t init() { return load_container_config(); }
            std::shared_ptr<nvidia_config> get_nvidia_config_from_cli();
            std::shared_ptr<container_config> get_container_config(std::shared_ptr<ai_training_task> task);
            nvidia_docker_version get_nv_docker_version();
            std::shared_ptr<container_client> get_worer_if() { return m_container_client;}
            int32_t can_pull_image();

        private:
            int32_t load_container_config();
            int32_t check_cpu_config(const int16_t & cpu_info);
            int32_t check_memory_config(const int16_t & memory, const int16_t & memory_swap, const int64_t & shm_size);

        private:
            variables_map m_container_args;
            const int64_t m_nano_cpu = 1000000000;
            const int64_t m_g_bytes = 1073741824;

            bool m_auto_pull_image = true;
            nvidia_docker_version m_nv_docker_version;

            std::string m_container_ip;
            uint16_t m_container_port;
            std::string m_docker_root_dir = "";
            
            //min disk_free 1024MB
            const uint32_t m_min_disk_free = 1024;

            std::shared_ptr<nvidia_config> m_nv_config;
            std::shared_ptr<container_client> m_nvidia_client;
            std::shared_ptr<container_client> m_container_client = nullptr;

            int64_t m_memory = 0;
            int64_t m_memory_swap = 0;
            int64_t m_shm_size = 0;
            int64_t m_nano_cpus = 0;
        };

    }

}
