/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_worker.cpp
* description    :   container_worker for definition
* date                  :   2018.10.26
* author            :   Regulus
**********************************************************************************/

#include "container_worker.h"
#include "common.h"
#include "document.h"

#include <event2/event.h>

#include <event2/buffer.h>
#include "error/en.h"
#include "oss_common_def.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "server.h"

#include <sstream>
#include <iostream>
#include <boost/property_tree/json_parser.hpp>

namespace ai
{
    namespace dbc
    {
        container_worker::container_worker()
            :m_container_ip(DEFAULT_LOCAL_IP)
            , m_container_port((uint16_t)std::stoi(DEFAULT_CONTAINER_LISTEN_PORT))
            , m_container_client(std::make_shared<container_client>(m_container_ip, m_container_port))
            , m_nv_docker_version(NVIDIA_DOCKER_UNKNOWN)
        {
        }

        int32_t container_worker::load_container_config()
        {
            ip_validator ip_vdr;
            port_validator port_vdr;

            //container ip
            const std::string & container_ip = CONF_MANAGER->get_container_ip();

            variable_value val;
            val.value() = container_ip;

            if (false == ip_vdr.validate(val))
            {
                LOG_ERROR << "ai power provider init conf invalid container ip: " << container_ip;
                return E_DEFAULT;
            }

            m_container_ip = container_ip;

            //container port
            std::string s_port = CONF_MANAGER->get_container_port();
            val.value() = s_port;

            if (false == port_vdr.validate(val))
            {
                LOG_ERROR << "ai power provider init conf invalid container port: " << s_port;
                return E_DEFAULT;
            }
            else
            {
                try
                {
                    m_container_port = (uint16_t)std::stoi(s_port);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "ai power provider service init conf container port: " << s_port << ", " << e.what();
                    return E_DEFAULT;
                }
            }

            m_container_client->set_address(m_container_ip, m_container_port);

            //file path
            const fs::path &container_path = env_manager::get_container_path();
            bpo::options_description container_opts("container file options");

            container_opts.add_options()
                ("memory", bpo::value<int16_t>()->default_value(90), "")
                ("memory_swap", bpo::value<int16_t>()->default_value(90), "")
                ("cpus", bpo::value<int16_t>()->default_value(90), "")
                ("shm_size", bpo::value<int64_t>()->default_value(0), "")
                ("host_volum_dir", bpo::value<std::string>()->default_value(""), "")
                ("auto_pull_image", bpo::value<bool>()->default_value(true), "")
                ("engine_reg", bpo::value<std::string>()->default_value(""), "");

            try
            {
                //container.conf
                std::ifstream conf_ifs(container_path.generic_string());
                bpo::store(bpo::parse_config_file(conf_ifs, container_opts), m_container_args);

                bpo::notify(m_container_args);

                std::string host_dir = m_container_args["host_volum_dir"].as<std::string>();

                if (!host_dir.empty() && !fs::exists(host_dir))
                {
                    LOG_ERROR << "host volum dir is not exist. pls check";
                    return E_DEFAULT;
                }

                if (!host_dir.empty() && !fs::is_empty(host_dir))
                {
                    LOG_WARNING << "dangerous: host_volum_dir contain files. pls check";
                }

                int32_t ret = check_cpu_config(m_container_args["cpus"].as<int16_t>());

                if (ret != E_SUCCESS)
                {
                    return ret;
                }

                ret = check_memory_config(m_container_args["memory"].as<int16_t>(), m_container_args["memory_swap"].as<int16_t>(), m_container_args["shm_size"].as<int64_t>());
                if (ret != E_SUCCESS)
                {
                    return ret;
                }
                m_auto_pull_image = m_container_args["auto_pull_image"].as<bool>();
                set_task_engine(m_container_args["engine_reg"].as<std::string>());
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "parse container.conf error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t container_worker::check_cpu_config(const int16_t & cpu_info)
        {
            if (cpu_info < 0 || cpu_info >100)
            {
                LOG_ERROR << "cpus config error. cpus value is in [0,100]";
                return E_DEFAULT;
            }
            uint16_t cpu_num = std::thread::hardware_concurrency();
            m_nano_cpus = cpu_info * m_nano_cpu * cpu_num / 100;

            return E_SUCCESS;
        }

        int32_t container_worker::check_memory_config(const int16_t & memory, const int16_t & memory_swap, const int64_t & shm_size)
        {
            if (memory < 0 || memory > 100)
            {
                LOG_ERROR << "memory config error. memeory value is in [0,100]";
                return E_DEFAULT;
            }

            if (memory_swap < 0 || memory_swap > 100)
            {
                LOG_ERROR << "memory_swap config error. memory_swap value is in [0,100]";
                return E_DEFAULT;
            }

            int64_t sys_mem = 0;
            int64_t sys_swap = 0;
            get_sys_mem(sys_mem, sys_swap);
            LOG_DEBUG << "system memory:" << sys_mem << " system swap memory" << sys_swap;

            if (0 == sys_mem || 0 == sys_swap)
            {
                return E_SUCCESS;
            }

            m_memory = memory * sys_mem / 100;
            m_memory_swap = memory_swap * sys_swap / 100;
            m_shm_size = shm_size * m_g_bytes;

            if (m_shm_size > sys_mem)
            {
                LOG_ERROR << "check shm_size failed.";
                return E_DEFAULT;
            }

            if (m_memory_swap !=0 && m_memory > m_memory_swap)
            {
                LOG_ERROR << "memory is bigger than memory_swap, pls check container.conf.system memory=" << sys_mem << " sys_swap=" << sys_swap 
                          << " container memory:" << m_memory << " container memory_swap:"<< m_memory_swap;
                return E_DEFAULT;
            }

            if (m_memory != 0 && m_shm_size > m_memory)
            {
                LOG_ERROR << "check shm_size failed. shm_size is bigger than memory";
                return E_DEFAULT;
            }

            LOG_DEBUG << "If container memory is 0, use default.container memory:" << m_memory << " container swap memory" << m_memory_swap;

            return E_SUCCESS;
        }
        std::string container_worker::get_operation(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }
            auto customer_setting=task->server_specification;
            std::string operation ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"operation\"):" << pt.count("operation");
                    if(pt.count("operation")!=0){
                        operation = pt.get<std::string>("operation");
                        LOG_INFO<< "operation: " << operation ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "operation: " << "error" ;
                }
            }

            return operation;
        }

        std::string container_worker::get_old_gpu_id(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }
            auto customer_setting=task->server_specification;
            std::string old_gpu_id ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"old_gpu_id\"):" << pt.count("old_gpu_id");
                    if(pt.count("old_gpu_id")!=0){
                        old_gpu_id = pt.get<std::string>("old_gpu_id");
                        LOG_INFO<< "old_gpu_id: " << old_gpu_id ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "old_gpu_id: " << "error" ;
                }
            }

            return old_gpu_id;
        }

        std::string container_worker::get_new_gpu_id(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }
            auto customer_setting=task->server_specification;
            std::string new_gpu_id ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"new_gpu_id\"):" << pt.count("new_gpu_id");
                    if(pt.count("new_gpu_id")!=0){
                        new_gpu_id = pt.get<std::string>("new_gpu_id");
                        LOG_INFO<< "new_gpu_id: " << new_gpu_id ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "new_gpu_id: " << "error" ;
                }
            }

            return new_gpu_id;
        }

        std::string container_worker::get_autodbcimage_version(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }
            auto customer_setting=task->server_specification;
            std::string autodbcimage_version ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;

                    LOG_INFO<< "pt.count(\"autodbcimage_version\"):" << pt.count("autodbcimage_version");
                    if(pt.count("autodbcimage_version")!=0){



                        autodbcimage_version = pt.get<std::string>("autodbcimage_version");

                        LOG_INFO<< "autodbcimage_version: " << autodbcimage_version;
                    }


                }
                catch (...)
                {
                    LOG_INFO<< "operation: " << "error" ;
                }
            }

            return autodbcimage_version;
        }


        std::shared_ptr<update_container_config> container_worker::get_update_container_config(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }

            std::shared_ptr<update_container_config> config = std::make_shared<update_container_config>();


          //  auto nv_docker_ver = get_nv_docker_version();

         //   if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
        //    {
                LOG_DEBUG << "get common attributes of nvidia docker";

                // jimmy: support NVIDIA GPU env configure from task spec
                std::map< std::string, std::string > env_map;
                env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
                env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

                auto customer_setting=task->server_specification;
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);

                        for (const auto& kv: pt.get_child("env")) {
                            env_map[kv.first.data()] = kv.second.data();
                            LOG_INFO << "[env] " << kv.first.data() << "="<<env_map[kv.first.data()];
                        }
                    }
                    catch (...)
                    {

                    }
                }


                for (auto & kv: env_map)
                {
                    config->env.push_back(kv.first+"=" + kv.second);
                }



//memory
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);
                        LOG_INFO<< "pt.count(\"memory\"):" << pt.count("memory");
                        if(pt.count("memory")!=0){
                            int64_t memory = pt.get<int64_t>("memory");
                            config->memory =memory;


                            LOG_INFO<< "memory: " << memory ;

                        }
                        LOG_INFO<< "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
                        if(pt.count("memory_swap")!=0){



                            int64_t memory_swap = pt.get<int64_t>("memory_swap");
                            config->memory_swap = memory_swap;
                            LOG_INFO<< "memory_swap: " << memory_swap;
                        }

                        LOG_INFO<< "pt.count(\"disk_quota\"):" << pt.count("disk_quota");
                        if(pt.count("disk_quota")!=0){



                            int64_t disk_quota = pt.get<int64_t>("disk_quota");
                            config->disk_quota = disk_quota;
                            LOG_INFO<< "disk_quota: " << disk_quota;
                        }

                        LOG_INFO<< "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
                        if(pt.count("cpu_shares")!=0){



                            int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                            config->cpu_shares = cpu_shares;
                            LOG_INFO<< "cpu_shares: " << cpu_shares;
                        }

                        LOG_INFO<< "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
                        if(pt.count("cpu_period")!=0){



                            int32_t cpu_period = pt.get<int32_t>("cpu_period");
                            config->cpu_period = cpu_period;
                            LOG_INFO<< "cpu_period: " << cpu_period;
                        }

                        LOG_INFO<< "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
                        if(pt.count("cpu_quota")!=0){



                            int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                            config->cpu_quota = cpu_quota;
                            LOG_INFO<< "cpu_quota: " << cpu_quota;
                        }


                        LOG_INFO<< "pt.count(\"gpus\"):" << pt.count("gpus");
                        if(pt.count("gpus")!=0){



                            int32_t gpus= pt.get<int32_t>("gpus");
                            config->gpus = gpus;
                            LOG_INFO<< "gpus: " << gpus;
                        }

                        LOG_INFO<< "pt.count(\"storage\"):" << pt.count("storage");
                        if(pt.count("storage")!=0){



                            std::string storage = pt.get<std::string>("storage");
                            config->storage = storage;
                            LOG_INFO<< "storage: " << storage;
                        }


                    }
                    catch (...)
                    {

                    }
                }



      //      }



            return config;
        }

        int64_t container_worker::get_sleep_time(std::shared_ptr<ai_training_task> task) {

            int64_t sleep_time=120;
            if (nullptr == task) {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return sleep_time;
            }


            auto customer_setting=task->server_specification;
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "pt.count(\"sleep_time\"):" << pt.count("sleep_time");
                    if(pt.count("sleep_time")!=0){


                        sleep_time = pt.get<int64_t>("sleep_time");

                        LOG_INFO<< "sleep_time: " << sleep_time;

                    }

                }
                catch (...)
                {

                }
            }

            return sleep_time;


        }



        std::shared_ptr<container_config> container_worker::get_container_config_from_image(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }

            std::shared_ptr<container_config> config = std::make_shared<container_config>();

            //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
            //dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py hyperparameters
            //download file + exec training
            std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
            exec_cmd += AI_TRAINING_TASK_SCRIPT;

            std::string start_cmd = task->entry_file + " " + task->hyper_parameters;

            container_mount mount1 ;
            mount1.target="/proc/cpuinfo";
            mount1.source="/var/lib/lxcfs/proc/cpuinfo";
            mount1.type="bind";
            mount1.read_only=false;
            mount1.consistency="consistent";

            container_mount mount2 ;
            mount2.target="/proc/diskstats";
            mount2.source="/var/lib/lxcfs/proc/diskstats";
            mount2.type="bind";
            mount2.read_only=false;
            mount2.consistency="consistent";

            container_mount mount3 ;
            mount3.target="/proc/meminfo";
            mount3.source="/var/lib/lxcfs/proc/meminfo";
            mount3.type="bind";
            mount3.read_only=false;
            mount3.consistency="consistent";

            container_mount mount4 ;
            mount4.target="/proc/swaps";
            mount4.source="/var/lib/lxcfs/proc/swaps";
            mount4.type="bind";
            mount4.read_only=false;
            mount4.consistency="consistent";

            config->host_config.mounts.push_back(mount1);
            config->host_config.mounts.push_back(mount2);
            config->host_config.mounts.push_back(mount3);
            config->host_config.mounts.push_back(mount4);
          //  config->volumes.dests.push_back("/proc/stat");
         //   config->volumes.binds.push_back("/var/lib/lxcfs/proc/stat");
          //  config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/swaps");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/swaps");
          //  config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/uptime");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/uptime");
          //  config->volumes.modes.push_back("rw");

            config->image = task->training_engine;
            config->cmd.push_back(exec_cmd);
            config->cmd.push_back(task->data_dir);
            config->cmd.push_back(task->code_dir);
            config->cmd.push_back(start_cmd);

            config->host_config.binds.push_back(AI_TRAINING_BIND_LOCALTIME);
            config->host_config.binds.push_back(AI_TRAINING_BIND_TIMEZONE);

            // config->host_config.memory = m_memory;
            // config->host_config.memory_swap = m_memory_swap;

            // config->host_config.memory =task->memory;
            //  config->host_config.memory_swap = task->memory_swap;
            config->host_config.nano_cpus = m_nano_cpus;

            std::string mount_dbc_data_dir = m_container_args["host_volum_dir"].as<std::string>();

            if (!mount_dbc_data_dir.empty())
            {
                mount_dbc_data_dir = mount_dbc_data_dir + ":" + "/dbc";
                config->host_config.binds.push_back(mount_dbc_data_dir);
            }

            std::string mount_dbc_utils_dir = env_manager::get_home_path().generic_string();

            if (!mount_dbc_utils_dir.empty())
            {
                // read only
                mount_dbc_utils_dir = mount_dbc_utils_dir + "/container:" + "/home/dbc_utils:ro";
                config->host_config.binds.push_back(mount_dbc_utils_dir);
            }


        //    auto nv_docker_ver = get_nv_docker_version();

       //     if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
       //     {
                LOG_DEBUG << "get common attributes of nvidia docker";

                // jimmy: support NVIDIA GPU env configure from task spec
                std::map< std::string, std::string > env_map;
                env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
                env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

                auto customer_setting=task->server_specification;
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);

                        for (const auto& kv: pt.get_child("env")) {
                            env_map[kv.first.data()] = kv.second.data();
                            LOG_INFO << "[env] " << kv.first.data() << "="<<env_map[kv.first.data()];
                        }
                    }
                    catch (...)
                    {

                    }
                }

//                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
//                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
                for (auto & kv: env_map)
                {
                    config->env.push_back(kv.first+"=" + kv.second);
                }

                config->host_config.share_memory = m_shm_size;
                config->host_config.ulimits.push_back(container_ulimits("memlock", -1, -1));

                // port binding
                container_port port;
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);

                        for (const auto& kv: pt.get_child("port")) {

                            container_port p;
                            p.scheme="tcp";
                            p.port=kv.first.data();
                            p.host_ip="";
                            p.host_port=kv.second.data();

                            config->host_config.port_bindings.ports[p.port] = p;

                            LOG_DEBUG << "[port] " << kv.first.data() << " = "<<p.host_port;
                        }
                    }
                    catch (...)
                    {

                    }
                }
//memory
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);
                        LOG_INFO<< "pt.count(\"memory\"):" << pt.count("memory");
                        if(pt.count("memory")!=0){
                            int64_t memory = pt.get<int64_t>("memory");
                            config->host_config.memory =memory;


                            LOG_INFO<< "memory: " << memory ;

                        }
                        LOG_INFO<< "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
                        if(pt.count("memory_swap")!=0){



                            int64_t memory_swap = pt.get<int64_t>("memory_swap");
                            config->host_config.memory_swap = memory_swap;
                            LOG_INFO<< "memory_swap: " << memory_swap;
                        }

                        LOG_INFO<< "pt.count(\"storage\"):" << pt.count("storage");
                        if(pt.count("storage")!=0){



                            std::string storage = pt.get<std::string>("storage");
                            config->host_config.storage = storage;
                            LOG_INFO<< "storage: " << storage;
                        }

                        LOG_INFO<< "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
                        if(pt.count("cpu_shares")!=0){



                            int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                            config->host_config.cpu_shares = cpu_shares;
                            LOG_INFO<< "cpu_shares: " << cpu_shares;
                        }


                        LOG_INFO<< "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
                        if(pt.count("cpu_period")!=0){



                            int32_t cpu_period = pt.get<int32_t>("cpu_period");
                            config->host_config.cpu_period = cpu_period;
                            LOG_INFO<< "cpu_period: " << cpu_period;
                        }

                        LOG_INFO<< "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
                        if(pt.count("cpu_quota")!=0){



                            int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                            config->host_config.cpu_quota = cpu_quota;
                            LOG_INFO<< "cpu_quota: " << cpu_quota;
                        }


                        LOG_INFO<< "pt.count(\"autodbcimage_version\"):" << pt.count("autodbcimage_version");
                        if(pt.count("autodbcimage_version")!=0){



                            int32_t autodbcimage_version = pt.get<int32_t>("autodbcimage_version");
                            config->host_config.autodbcimage_version = autodbcimage_version;
                            LOG_INFO<< "autodbcimage_version: " << autodbcimage_version;
                        }


                        LOG_INFO<< "pt.count(\"privileged\"):" << pt.count("privileged");
                        if(pt.count("privileged")!=0){



                            std::string privileged = pt.get<std::string>("privileged");
                            if(privileged.compare("false")==0){
                                config->host_config.privileged =false;
                            } else{

                                config->host_config.privileged =true;

                            }

                        }



                    }
                    catch (...)
                    {

                    }
                }

                // LOG_INFO << "config->host_config.memory"+config->host_config.memory;

        //    }

        /*    switch (nv_docker_ver)
            {
                case NVIDIA_DOCKER_TWO:
                {
                    LOG_INFO << "nvidia docker 2.x";
                    config->host_config.runtime = RUNTIME_NVIDIA;
                    break;
                }
                default:
                {
                    LOG_INFO << "not find nvidia docker";
                }
            }*/
            config->host_config.runtime = RUNTIME_NVIDIA;
            return config;
        }



        std::shared_ptr<container_config> container_worker::get_container_config(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }

            std::shared_ptr<container_config> config = std::make_shared<container_config>();

            //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
            //dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py hyperparameters
            //download file + exec training 
            std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
            exec_cmd += AI_TRAINING_TASK_SCRIPT;

            std::string start_cmd = task->entry_file + " " + task->hyper_parameters;


          //  config->volumes.dests.push_back("/proc/cpuinfo");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/cpuinfo");
         //   config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/diskstats");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/diskstats");
          //  config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/meminfo");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/meminfo");
          //  config->volumes.modes.push_back("rw");

            container_mount mount1 ;
            mount1.target="/proc/cpuinfo";
            mount1.source="/var/lib/lxcfs/proc/cpuinfo";
            mount1.type="bind";
            mount1.read_only=false;
            mount1.consistency="consistent";

            container_mount mount2 ;
            mount2.target="/proc/diskstats";
            mount2.source="/var/lib/lxcfs/proc/diskstats";
            mount2.type="bind";
            mount2.read_only=false;
            mount2.consistency="consistent";

            container_mount mount3 ;
            mount3.target="/proc/meminfo";
            mount3.source="/var/lib/lxcfs/proc/meminfo";
            mount3.type="bind";
            mount3.read_only=false;
            mount3.consistency="consistent";

            container_mount mount4 ;
            mount4.target="/proc/swaps";
            mount4.source="/var/lib/lxcfs/proc/swaps";
            mount4.type="bind";
            mount4.read_only=false;
            mount4.consistency="consistent";

            config->host_config.mounts.push_back(mount1);
            config->host_config.mounts.push_back(mount2);
            config->host_config.mounts.push_back(mount3);
            config->host_config.mounts.push_back(mount4);
          //  config->volumes.dests.push_back("/proc/stat");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/stat");
          //  config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/swaps");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/swaps");
          //  config->volumes.modes.push_back("rw");

          //  config->volumes.dests.push_back("/proc/uptime");
          //  config->volumes.binds.push_back("/var/lib/lxcfs/proc/uptime");
          //  config->volumes.modes.push_back("rw");

            config->image = task->training_engine;
            config->cmd.push_back(exec_cmd);
            config->cmd.push_back(task->data_dir);
            config->cmd.push_back(task->code_dir);
            config->cmd.push_back(start_cmd);

            config->host_config.binds.push_back(AI_TRAINING_BIND_LOCALTIME);
            config->host_config.binds.push_back(AI_TRAINING_BIND_TIMEZONE);

           // config->host_config.memory = m_memory;
           // config->host_config.memory_swap = m_memory_swap;

           // config->host_config.memory =task->memory;
           //  config->host_config.memory_swap = task->memory_swap;
            config->host_config.nano_cpus = m_nano_cpus;

            std::string mount_dbc_data_dir = m_container_args["host_volum_dir"].as<std::string>();

            if (!mount_dbc_data_dir.empty())
            {
                mount_dbc_data_dir = mount_dbc_data_dir + ":" + "/dbc";
                config->host_config.binds.push_back(mount_dbc_data_dir);
            }

            std::string mount_dbc_utils_dir = env_manager::get_home_path().generic_string();

            if (!mount_dbc_utils_dir.empty())
            {
                // read only
                mount_dbc_utils_dir = mount_dbc_utils_dir + "/container:" + "/home/dbc_utils:ro";
                config->host_config.binds.push_back(mount_dbc_utils_dir);
            }


          //  auto nv_docker_ver = get_nv_docker_version();

         //   if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
         //   {
                LOG_DEBUG << "get common attributes of nvidia docker";

                // jimmy: support NVIDIA GPU env configure from task spec
                std::map< std::string, std::string > env_map;
                env_map["NVIDIA_VISIBLE_DEVICES"] = "all";
                env_map["NVIDIA_DRIVER_CAPABILITIES"] = "compute,utility";

                auto customer_setting=task->server_specification;
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);

                        for (const auto& kv: pt.get_child("env")) {
                            env_map[kv.first.data()] = kv.second.data();
                            LOG_INFO << "[env] " << kv.first.data() << "="<<env_map[kv.first.data()];
                        }
                    }
                    catch (...)
                    {

                    }
                }

//                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
//                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
                for (auto & kv: env_map)
                {
                    config->env.push_back(kv.first+"=" + kv.second);
                }

                config->host_config.share_memory = m_shm_size;
                config->host_config.ulimits.push_back(container_ulimits("memlock", -1, -1));

                // port binding
                container_port port;
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);

                        for (const auto& kv: pt.get_child("port")) {

                            container_port p;
                            p.scheme="tcp";
                            p.port=kv.first.data();
                            p.host_ip="";
                            p.host_port=kv.second.data();

                            config->host_config.port_bindings.ports[p.port] = p;

                            LOG_DEBUG << "[port] " << kv.first.data() << " = "<<p.host_port;
                        }
                    }
                    catch (...)
                    {

                    }
                }
//memory
                if (!customer_setting.empty())
                {
                    std::stringstream ss;
                    ss << customer_setting;
                    boost::property_tree::ptree pt;

                    try
                    {
                        boost::property_tree::read_json(ss, pt);
                        LOG_INFO<< "pt.count(\"memory\"):" << pt.count("memory");
                        if(pt.count("memory")!=0){
                            int64_t memory = pt.get<int64_t>("memory");
                            config->host_config.memory =memory;


                            LOG_INFO<< "memory: " << memory ;

                        }
                        LOG_INFO<< "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
                        if(pt.count("memory_swap")!=0){



                            int64_t memory_swap = pt.get<int64_t>("memory_swap");
                            config->host_config.memory_swap = memory_swap;
                            LOG_INFO<< "memory_swap: " << memory_swap;
                        }

                        LOG_INFO<< "pt.count(\"storage\"):" << pt.count("storage");
                        if(pt.count("storage")!=0){



                            std::string storage = pt.get<std::string>("storage");
                            config->host_config.storage = storage;
                            LOG_INFO<< "storage: " << storage;
                        }

                        LOG_INFO<< "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
                        if(pt.count("cpu_shares")!=0){



                            int32_t cpu_shares = pt.get<int32_t>("cpu_shares");
                            config->host_config.cpu_shares = cpu_shares;
                            LOG_INFO<< "cpu_shares: " << cpu_shares;
                        }

                        LOG_INFO<< "pt.count(\"cpu_period\"):" << pt.count("cpu_period");
                        if(pt.count("cpu_period")!=0){



                            int32_t cpu_period = pt.get<int32_t>("cpu_period");
                            config->host_config.cpu_period = cpu_period;
                            LOG_INFO<< "cpu_period: " << cpu_period;
                        }

                        LOG_INFO<< "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
                        if(pt.count("cpu_quota")!=0){



                            int32_t cpu_quota = pt.get<int32_t>("cpu_quota");
                            config->host_config.cpu_quota = cpu_quota;
                            LOG_INFO<< "cpu_quota: " << cpu_quota;
                        }

                        LOG_INFO<< "pt.count(\"disk_quota\"):" << pt.count("disk_quota");
                        if(pt.count("disk_quota")!=0){



                            int64_t disk_quota = pt.get<int64_t>("disk_quota");
                            config->host_config.disk_quota = disk_quota;
                            LOG_INFO<< "disk_quota: " << disk_quota;
                        }

                        LOG_INFO<< "pt.count(\"privileged\"):" << pt.count("privileged");
                        if(pt.count("privileged")!=0){



                            std::string privileged = pt.get<std::string>("privileged");
                            if(privileged.compare("false")==0){
                                config->host_config.privileged =false;
                            } else{

                                config->host_config.privileged =true;
                               // config->entry_point.push_back("/sbin/init;");

                            }

                        }

                    }
                    catch (...)
                    {

                    }
                }

               // LOG_INFO << "config->host_config.memory"+config->host_config.memory;

       //     }

          /*  switch (nv_docker_ver)
            {
            case NVIDIA_DOCKER_TWO:
            {
                LOG_INFO << "nvidia docker 2.x";
                config->host_config.runtime = RUNTIME_NVIDIA;
                break;
            }
            default:
            {
                LOG_INFO << "not find nvidia docker";
            }
            }*/
            config->host_config.runtime = RUNTIME_NVIDIA;
            return config;
        }


        /**
        *
        * @return nvidia docker version in the host
        */
        nvidia_docker_version container_worker::get_nv_docker_version()
        {
            if (m_nv_docker_version != NVIDIA_DOCKER_UNKNOWN)
            {
                return m_nv_docker_version;
            }

            {
                // nvidia-docker 2.x add a new docker runtime named nvidia. Check if nvidia runtime exists.
                auto docker_info_ptr = m_container_client->get_docker_info();
                if (docker_info_ptr && docker_info_ptr->runtimes.count(RUNTIME_NVIDIA))
                {
                    m_nv_docker_version = NVIDIA_DOCKER_TWO;
                }
            }

            return m_nv_docker_version;
        }

        int32_t container_worker::can_pull_image()
        {
            //if do not allow auto pull image, then pop task directly
            if (!m_auto_pull_image)
            {
                LOG_ERROR << "docker image do not exist, should config auto_pull_image=true, or pull image manually.";
                return E_PULLING_IMAGE;
            }

            //get docker root dir
            if (m_docker_root_dir.empty())
            {
                std::shared_ptr<docker_info> docker_info_ptr = m_container_client->get_docker_info();
                if (!docker_info_ptr)
                {
                    LOG_ERROR << "docker get docker info faild.";
                    return E_DEFAULT;
                }
                m_docker_root_dir = docker_info_ptr->root_dir;
            }

            //if the docker partion do not have enough space, return error
            if (get_disk_free(m_docker_root_dir) < m_min_disk_free)
            {
                LOG_ERROR << "docker get docker info faild. the disk have no enough space. the free space: " << get_disk_free(m_docker_root_dir) << "MB";
                return E_NO_DISK_SPACE;
            }

            return E_SUCCESS;
        }

        std::string container_worker::get_old_memory(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return "";
            }
            auto customer_setting=task->server_specification;
            std::string old_memory ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"old_memory\"):" << pt.count("old_memory");
                    if(pt.count("old_memory")!=0){
                        old_memory = pt.get<std::string>("old_memory");
                        LOG_INFO<< "old_memory: " << old_memory ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "old_memory: " << "error" ;
                }
            }

            return old_memory;
        }

        std::string container_worker::get_new_memory(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return "";
            }
            auto customer_setting=task->server_specification;
            std::string memory ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"memory\"):" << pt.count("memory");
                    if(pt.count("memory")!=0){
                        memory = pt.get<std::string>("memory");
                        LOG_INFO<< "memory: " << memory ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "memory: " << "error" ;
                }
            }

            return memory;
        }


        std::string container_worker::get_old_memory_swap(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return "";
            }
            auto customer_setting=task->server_specification;
            std::string old_memory_swap ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"old_memory_swap\"):" << pt.count("old_memory_swap");
                    if(pt.count("old_memory_swap")!=0){
                        old_memory_swap = pt.get<std::string>("old_memory_swap");
                        LOG_INFO<< "old_memory_swap: " << old_memory_swap ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "old_memory_swap: " << "error" ;
                }
            }

            return old_memory_swap;
        }

        std::string container_worker::get_new_memory_swap(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return "";
            }
            auto customer_setting=task->server_specification;
            std::string memory_swap ="";
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"memory_swap\"):" << pt.count("memory_swap");
                    if(pt.count("memory_swap")!=0){
                        memory_swap = pt.get<std::string>("memory_swap");
                        LOG_INFO<< "memory_swap: " << memory_swap ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "memory_swap: " << "error" ;
                }
            }

            return memory_swap;
        }


        int32_t container_worker::get_old_cpu_shares(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return 0;
            }
            auto customer_setting=task->server_specification;
            int32_t old_cpu_shares =0;
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"old_cpu_shares\"):" << pt.count("old_cpu_shares");
                    if(pt.count("old_cpu_shares")!=0){
                        old_cpu_shares = pt.get<int32_t>("old_cpu_shares");
                        LOG_INFO<< "old_cpu_shares: " << old_cpu_shares ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "old_cpu_shares: " << "error" ;
                }
            }

            return old_cpu_shares;
        }

        int32_t container_worker::get_new_cpu_shares(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return 0;
            }
            auto customer_setting=task->server_specification;
            int32_t cpu_shares =0;
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"cpu_shares\"):" << pt.count("cpu_shares");
                    if(pt.count("cpu_shares")!=0){
                        cpu_shares = pt.get<int32_t>("cpu_shares");
                        LOG_INFO<< "cpu_shares: " << cpu_shares ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "cpu_shares: " << "error" ;
                }
            }

            return cpu_shares;
        }


        int32_t container_worker::get_old_cpu_quota(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return 0;
            }
            auto customer_setting=task->server_specification;
            int32_t old_cpu_quota =0;
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"old_cpu_quota\"):" << pt.count("old_cpu_quota");
                    if(pt.count("old_cpu_quota")!=0){
                        old_cpu_quota = pt.get<int32_t>("old_cpu_quota");
                        LOG_INFO<< "old_cpu_quota: " << old_cpu_quota ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "old_cpu_quota: " << "error" ;
                }
            }

            return old_cpu_quota;
        }

        int32_t container_worker::get_new_cpu_quota(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {

                return 0;
            }
            auto customer_setting=task->server_specification;
            int32_t cpu_quota =0;
            if (!customer_setting.empty())
            {
                std::stringstream ss;
                ss << customer_setting;
                boost::property_tree::ptree pt;

                try
                {
                    boost::property_tree::read_json(ss, pt);
                    LOG_INFO<< "task->server_specification" << task->server_specification;
                    LOG_INFO<< "pt.count(\"cpu_quota\"):" << pt.count("cpu_quota");
                    if(pt.count("cpu_quota")!=0){
                        cpu_quota = pt.get<int32_t>("cpu_quota");
                        LOG_INFO<< "cpu_quota: " << cpu_quota ;

                    }


                }
                catch (...)
                {
                    LOG_INFO<< "cpu_quota: " << "error" ;
                }
            }

            return cpu_quota;
        }

    }
}
