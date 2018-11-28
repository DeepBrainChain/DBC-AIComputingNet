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
namespace ai
{
    namespace dbc
    {
        container_worker::container_worker()
            :m_container_ip(DEFAULT_LOCAL_IP)
            , m_container_port((uint16_t)std::stoi(DEFAULT_CONTAINER_LISTEN_PORT))
            , m_container_client(std::make_shared<container_client>(m_container_ip, m_container_port))
            , m_nvidia_client(std::make_shared<container_client>(m_container_ip, DEFAULT_NVIDIA_DOCKER_PORT))
            , m_nv_config(nullptr)
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
            m_nvidia_client->set_address(m_container_ip, DEFAULT_NVIDIA_DOCKER_PORT);

            //file path
            const fs::path &container_path = env_manager::get_container_path();
            bpo::options_description container_opts("container file options");

            container_opts.add_options()
                ("memory", bpo::value<double>()->default_value(0.9), "")
                ("memory_swap", bpo::value<double>()->default_value(0.9), "")
                ("cpus", bpo::value<double>()->default_value(0.9), "")
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

                int32_t ret = check_cpu_config(m_container_args["cpus"].as<double>());

                if (ret != E_SUCCESS)
                {
                    return ret;
                }

                ret = check_memory_config(m_container_args["memory"].as<double>(), m_container_args["memory_swap"].as<double>(), m_container_args["shm_size"].as<int64_t>());
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

        int32_t container_worker::check_cpu_config(const double & cpu_info)
        {
            if (cpu_info < 0 || cpu_info >1)
            {
                LOG_ERROR << "cpus config error. cpus should config 0~1";
                return E_DEFAULT;
            }
            uint16_t cpu_num = std::thread::hardware_concurrency();
            m_nano_cpus = cpu_info * m_nano_cpu * cpu_num;

            return E_SUCCESS;
        }

        int32_t container_worker::check_memory_config(const double & memory, const double & memory_swap, const int64_t & shm_size)
        {
            if (memory < 0 || memory > 1)
            {
                LOG_ERROR << "memory config error. memeory value is in [0,1]";
                return E_DEFAULT;
            }

            if (memory_swap < 0 || memory_swap > 1)
            {
                LOG_ERROR << "memory_swap config error. memory_swap value is in [0,1]";
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

            

            m_memory = memory * sys_mem;
            m_memory_swap = memory_swap * sys_swap;
            m_shm_size = shm_size * m_g_bytes;

            if (m_shm_size > sys_mem)
            {
                LOG_ERROR << "check shm_size failed.";
                return E_DEFAULT;
            }

            if (m_memory_swap !=0 && m_memory > m_memory_swap)
            {
                LOG_ERROR << "memory is bigger than memory_swap. system memory=" << sys_mem << " sys_swap=" << sys_swap;
                return E_DEFAULT;
            }

            if (memory != 0 && m_shm_size > m_memory)
            {
                LOG_ERROR << "check shm_size failed. shm_size is bigger than memory";
                return E_DEFAULT;
            }

            LOG_DEBUG << "container memory:" << m_memory << " container swap memory" << m_memory_swap;

            return E_SUCCESS;
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

            config->image = task->training_engine;
            config->cmd.push_back(exec_cmd);
            config->cmd.push_back(task->data_dir);
            config->cmd.push_back(task->code_dir);
            config->cmd.push_back(start_cmd);

            config->host_config.binds.push_back(AI_TRAINING_BIND_LOCALTIME);
            config->host_config.binds.push_back(AI_TRAINING_BIND_TIMEZONE);

            config->host_config.memory = m_memory;
            config->host_config.memory_swap = m_memory_swap;
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


            auto nv_docker_ver = get_nv_docker_version();

            if (nv_docker_ver != NVIDIA_DOCKER_UNKNOWN)
            {
                LOG_DEBUG << "get common attributes of nvidia docker";
                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
                config->host_config.share_memory = m_shm_size;
                config->host_config.ulimits.push_back(container_ulimits("memlock", -1, -1));
            }

            switch (nv_docker_ver)
            {
            case NVIDIA_DOCKER_ONE:
            {
                std::shared_ptr<nvidia_config> nv_config = get_nvidia_config_from_cli();

                if (!nv_config) break;

                LOG_INFO << "nvidia docker 1.x";

                //binds
                config->host_config.binds.push_back(nv_config->volume);

                //devices
                for (auto it = nv_config->devices.begin(); it != nv_config->devices.end(); it++)
                {
                    container_device dev;

                    dev.path_on_host = *it;
                    dev.path_in_container = *it;
                    dev.cgroup_permissions = AI_TRAINING_CGROUP_PERMISSIONS;

                    config->host_config.devices.push_back(dev);
                }

                //VolumeDriver
                config->host_config.volume_driver = nv_config->driver_name;

                //Mounts
                /*container_mount nv_mount;
                nv_mount.type = "volume";

                std::vector<std::string> vec;
                string_util::split(nv_config->driver_name, ":", vec);
                if (vec.size() > 0)
                {
                nv_mount.name = vec[0];
                }
                else
                {
                LOG_ERROR << "container config get mounts name from nv_config error";
                return nullptr;
                }

                nv_mount.source = AI_TRAINING_MOUNTS_SOURCE;
                nv_mount.destination = AI_TRAINING_MOUNTS_DESTINATION;
                nv_mount.driver = nv_config->driver_name;
                nv_mount.mode = AI_TRAINING_MOUNTS_MODE;
                nv_mount.rw = false;
                nv_mount.propagation = DEFAULT_STRING;

                config->host_config.mounts.push_back(nv_mount);*/
                break;
            }
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
            }

            return config;
        }

        std::shared_ptr<nvidia_config> container_worker::get_nvidia_config_from_cli()
        {
            //already have
            if (nullptr != m_nv_config)
            {
                return m_nv_config;
            }

            if (nullptr == m_nvidia_client)
            {
                LOG_ERROR << "nvidia client is nullptr";
                return nullptr;
            }

            std::shared_ptr<nvidia_config_resp> resp = m_nvidia_client->get_nvidia_config();
            if (nullptr == resp)
            {
                LOG_ERROR << "nvidia client get nvidia config error";
                return nullptr;
            }

            std::vector<std::string> vec;
            string_util::split(resp->content, " ", vec);
            if (0 == vec.size())
            {
                LOG_ERROR << "nvidia client get nvidia config split content into vector error";
                return nullptr;
            }

            m_nv_config = std::make_shared<nvidia_config>();

            //get nv gpu config
            for (auto it = vec.begin(); it != vec.end(); it++)
            {
                std::vector<std::string> v;
                std::string::size_type pos = std::string::npos;

                //--volume-driver
                pos = it->find("--volume-driver");
                if (std::string::npos != pos)
                {
                    string_util::split(*it, "=", v);
                    if (DEFAULT_SPLIT_COUNT == v.size())
                    {
                        m_nv_config->driver_name = v[1];
                        LOG_DEBUG << "--volume-driver: " << m_nv_config->driver_name;
                    }
                    continue;
                }

                //--volume
                pos = it->find("--volume");
                if (std::string::npos != pos)
                {
                    string_util::split(*it, "=", v);
                    if (DEFAULT_SPLIT_COUNT == v.size())
                    {
                        m_nv_config->volume = v[1];
                        LOG_DEBUG << "--volume: " << m_nv_config->volume;
                    }
                    continue;
                }

                //--device
                pos = it->find("--device");
                if (std::string::npos != pos)
                {
                    string_util::split(*it, "=", v);
                    if (DEFAULT_SPLIT_COUNT == v.size())
                    {
                        m_nv_config->devices.push_back(v[1]);
                        LOG_DEBUG << "--device: " << v[1];
                    }
                    continue;
                }
            }

            /*m_nv_config->driver_name = "nvidia-docker";
            m_nv_config->volume = "nvidia_driver_384.111:/usr/local/nvidia:ro";

            m_nv_config->devices.push_back("/dev/nvidiactl");
            m_nv_config->devices.push_back("/dev/nvidia-uvm");
            m_nv_config->devices.push_back("/dev/nvidia-uvm-tools");
            m_nv_config->devices.push_back("/dev/nvidia0");*/

            return m_nv_config;
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

            if (get_nvidia_config_from_cli() != nullptr)
            {
                // nvidia-docker 1.x added a plug-in on runc.
                m_nv_docker_version = NVIDIA_DOCKER_ONE;
            }
            else
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
    }

}