/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ai_power_provider_service.cpp
* description    ai_power_provider_service
* date                  : 2018.01.28
* author            Bruce Feng
**********************************************************************************/
#include <cassert>
#include <boost/exception/all.hpp>
#include <iostream>
#include "server.h"
#include "api_call_handler.h"
#include "conf_manager.h"
#include "ai_power_provider_service.h"
#include "peer_node.h"
#include "service_message_id.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "port_validator.h"
#include "task_common_def.h"
#include "util.h"


using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace matrix::service_core;
using namespace ai::dbc;


namespace ai
{
    namespace dbc
    {

        ai_power_provider_service::ai_power_provider_service()
            : m_prov_training_task_db()
            , m_container_ip(DEFAULT_LOCAL_IP)
            , m_container_port((uint16_t)std::stoi(DEFAULT_CONTAINER_LISTEN_PORT))
            , m_container_client(std::make_shared<container_client>(m_container_ip, m_container_port))
            , m_nvidia_client(std::make_shared<container_client>(m_container_ip, DEFAULT_NVIDIA_DOCKER_PORT))
            , m_training_task_timer_id(INVALID_TIMER_ID)
            , m_nv_config(nullptr)
        {

        }

        int32_t ai_power_provider_service::init_conf()
        {
            port_validator port_vdr;

            if (CONF_MANAGER->count("container_ip"))
            {
                m_container_ip = (*CONF_MANAGER)["container_ip"].as<std::string>();
            }

            if (CONF_MANAGER->count("container_port"))
            {
                std::string s_port = (*CONF_MANAGER)["container_port"].as<std::string>();

                variable_value val;
                val.value() = s_port;

                if (false == port_vdr.validate(val))
                {
                    LOG_ERROR << "p2p_net_service init_conf invalid main net port: " << s_port;
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
                        LOG_ERROR << "p2p_net_service init_conf invalid main_port: " << s_port << ", " << e.what();
                        return E_DEFAULT;
                    }
                }

            }

            if (CONF_MANAGER->count("container_image"))
            {
                m_container_image = (*CONF_MANAGER)["container_image"].as<std::string>();
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            ret = init_conf();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power provider service init config error";
                return ret;
            }

            ret = init_db();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power provider service init db error";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::service_exit()
        {
            remove_timer(m_training_task_timer_id);
            return E_SUCCESS;
        }

        void ai_power_provider_service::init_subscription()
        {
            //ai training
            SUBSCRIBE_BUS_MESSAGE(AI_TRAINING_NOTIFICATION_REQ);

            //stop training
            SUBSCRIBE_BUS_MESSAGE(STOP_TRAINING_REQ);

            //list training req
            SUBSCRIBE_BUS_MESSAGE(LIST_TRAINING_REQ);

            //task logs req
            SUBSCRIBE_BUS_MESSAGE(LOGS_REQ);
        }

        void ai_power_provider_service::init_invoker()
        {
            invoker_type invoker;

            //ai training
            BIND_MESSAGE_INVOKER(AI_TRAINING_NOTIFICATION_REQ, &ai_power_provider_service::on_start_training_req);

            //stop training
            BIND_MESSAGE_INVOKER(STOP_TRAINING_REQ, &ai_power_provider_service::on_stop_training_req);

            //list training req
            BIND_MESSAGE_INVOKER(LIST_TRAINING_REQ, &ai_power_provider_service::on_list_training_req);

            //task logs req
            BIND_MESSAGE_INVOKER(LOGS_REQ, &ai_power_provider_service::on_logs_req);
        }

        int32_t ai_power_provider_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;

            //get db path
            fs::path task_db_path = env_manager::get_db_path();
            if (false == fs::exists(task_db_path))
            {
                LOG_DEBUG << "db directory path does not exist and create db directory";
                fs::create_directory(task_db_path);
            }

            //check db directory
            if (false == fs::is_directory(task_db_path))
            {
                LOG_ERROR << "db directory path does not exist and exit";
                return E_DEFAULT;
            }

            task_db_path /= fs::path("prov_training_task.db");
            LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

            //open db
            leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
            if (false == status.ok())
            {
                LOG_ERROR << "ai power provider service init training task db error: " << status.ToString();
                return E_DEFAULT;
            }

            //smart point auto close db
            m_prov_training_task_db.reset(db);

            //load task
            load_task_from_db();
            return E_SUCCESS;
        }

        void ai_power_provider_service::init_timer()
        {
            m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&ai_power_provider_service::on_training_task_timer, this, std::placeholders::_1);
            m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER, AI_TRAINING_TASK_TIMER_INTERVAL);
            assert(INVALID_TIMER_ID != m_training_task_timer_id);
        }

        int32_t ai_power_provider_service::on_start_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::start_training_req> req = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(msg->get_content());
            assert(nullptr != req);

            //relay start training in network
            LOG_DEBUG << "ai power provider service relay broadcast start training req to neighbor peer nodes: " << req->body.task_id;
            CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);

            //check node id
            const std::vector<std::string> &peer_nodes = req->body.peer_nodes_list;
            auto it = peer_nodes.begin();
            for (; it != peer_nodes.end(); it++)
            {
                if ((*it) == CONF_MANAGER->get_node_id())
                {
                    LOG_DEBUG << "ai power provider service found self node id in on start training: " << req->body.task_id << " node id: " << (*it);
                    break;
                }
            }

            //not found self just return
            if (it == peer_nodes.end())
            {
                LOG_DEBUG << "ai power provider service found start training req " << req->body.task_id << " is not self and exit function";
                return E_SUCCESS;
            }

            if (nullptr == m_prov_training_task_db)
            {
                LOG_ERROR << "ai power provider service training task db is nullptr";
                return E_DEFAULT;
            }

            //check task id exists 
            std::string task_value;
            leveldb::Status status = m_prov_training_task_db->Get(leveldb::ReadOptions(), req->body.task_id, &task_value);
            if (status.ok())                //already exists and directly return
            {
                LOG_DEBUG << "ai power provider service on start training already had task: " << req->body.task_id;
                return E_SUCCESS;
            }

            assert(0 == m_training_tasks.count(req->body.task_id));

            std::shared_ptr<ai_training_task> task = std::make_shared<ai_training_task>();
            assert(nullptr != task);

            task->task_id = req->body.task_id;
            task->select_mode = req->body.select_mode;
            task->master = req->body.master;
            task->peer_nodes_list.swap(req->body.peer_nodes_list);
            task->server_specification = req->body.server_specification;
            task->server_count = req->body.server_count;
            task->training_engine = req->body.training_engine;
            task->code_dir = req->body.code_dir;
            task->entry_file = req->body.entry_file;
            task->data_dir = req->body.data_dir;
            task->checkpoint_dir = req->body.checkpoint_dir;
            task->hyper_parameters = req->body.hyper_parameters;

            task->retry_times = 0;
            task->container_id = "";
            task->received_time_stamp = std::time(nullptr);
            task->status = task_queueing;

            //flush to db
            write_task_to_db(task);
            LOG_DEBUG << "ai power provider service flush task to db: " << req->body.task_id;

            //add to task queue
            m_queueing_tasks.push_back(task);
            m_training_tasks[task->task_id] = task;

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_stop_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<matrix::service_core::stop_training_req> req = std::dynamic_pointer_cast<matrix::service_core::stop_training_req>(msg->get_content());
            assert(nullptr != req);

            //relay on stop_training to network(maybe task running on multiple nodes)
            LOG_DEBUG << "ai power provider service relay broadcast stop_training req to neighbor peer nodes: " << req->body.task_id;
            CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);

            //check task_id
            if (0 == m_queueing_tasks.size())
            {
                LOG_DEBUG << "ai power provider service training queuing task is empty";
                return E_SUCCESS;
            }

            std::shared_ptr<ai_training_task> sp_task;
            const std::string& task_id = std::dynamic_pointer_cast<stop_training_req>(msg->get_content())->body.task_id;

            //find from queue
            auto it = m_queueing_tasks.begin();
            for (; it != m_queueing_tasks.end(); it++)
            {
                if ((*it)->task_id == task_id)
                {
                    sp_task = *it;
                    break;
                }
            }

            if (sp_task) //found
            {
                LOG_DEBUG << "stop training, task_status: " << sp_task->status << endl;

                if (task_running == sp_task->status)
                {
                    //stop container
                    int32_t ret = m_container_client->stop_container(sp_task->container_id);
                    if (E_SUCCESS != ret)
                    {
                        LOG_ERROR << "ai power provider service stop container error, container id: " << sp_task->container_id;
                    }
                }

                //remove from queue
                m_queueing_tasks.erase(it);

                //flush to db: update status
                sp_task->status = task_stopped;
                write_task_to_db(sp_task);

            }
            else
            {
                LOG_DEBUG << "stop training, not found task: " << task_id << endl;
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_list_training_req(std::shared_ptr<message> &msg)
        {
            LOG_DEBUG << "ai power provider service relay broadcast list_training req to neighbor peer nodes";
            std::shared_ptr<list_training_req> req_content = std::dynamic_pointer_cast<list_training_req>(msg->get_content());
            assert(nullptr != req_content);

            if (req_content->body.task_list.size() == 0)
            {
                LOG_DEBUG << "ai power provider service recv empty list training tasks";
                return E_DEFAULT;
            }

            //relay list_training to network(maybe task running on multiple nodes, no mater I took this task)
            CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);

            if (0 == m_training_tasks.size())
            {
                LOG_DEBUG << "ai power provider service training queuing task is empty";
                return E_SUCCESS;
            }

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::make_shared<matrix::service_core::list_training_resp>();

            int32_t count = 0;
            for (auto tid : req_content->body.task_list)
            {
                //find task
                auto it = m_training_tasks.find(tid);
                if (it == m_training_tasks.end())
                {
                    continue;
                }

                matrix::service_core::task_status ts;
                ts.task_id = it->second->task_id;
                ts.status = it->second->status;
                rsp_content->body.task_status_list.push_back(ts);

                //should restrict max count
                if (++count > MAX_LIST_TASK_COUNT)
                {
                    break;
                }
            }

            if (!rsp_content->body.task_status_list.empty())
            {
                //content header
                rsp_content->header.magic = TEST_NET;
                rsp_content->header.msg_name = LIST_TRAINING_RESP;
                rsp_content->header.__set_nonce(id_generator().generate_nonce());
                rsp_content->header.__set_session_id(req_content->header.session_id);

                //resp msg
                std::shared_ptr<message> resp_msg = std::make_shared<message>();
                resp_msg->set_name(LIST_TRAINING_RESP);
                resp_msg->set_content(std::dynamic_pointer_cast<base>(rsp_content));
                CONNECTION_MANAGER->broadcast_message(resp_msg);
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_logs_req(const std::shared_ptr<message> &msg)
        {
            std::shared_ptr<logs_req> req_content = std::dynamic_pointer_cast<logs_req>(msg->get_content());
            assert(nullptr != req_content);

            const std::string &task_id = req_content->body.task_id;

            //check log direction
            if (GET_LOG_HEAD != req_content->body.head_or_tail && GET_LOG_TAIL != req_content->body.head_or_tail)
            {
                LOG_DEBUG << "ai power provider service on logs req log direction error: " << task_id;
                return E_DEFAULT;
            }

            //check number of lines
            if (req_content->body.number_of_lines > MAX_NUMBER_OF_LINES)
            {
                LOG_DEBUG << "ai power provider service on logs req number of lines error: " << req_content->body.number_of_lines;
                return E_DEFAULT;
            }

            //check task id
            /*std::string task_value;
            leveldb::Status status = m_prov_training_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
            if (!status.ok())
            {
                //relay msg to network
                LOG_DEBUG << "ai power provider service on logs req does not have task: " << task_id;
                CONNECTION_MANAGER->broadcast_message(msg);
                return E_SUCCESS;
            }*/

            //check task id and get container
            auto it = m_training_tasks.find(task_id);
            if (it == m_training_tasks.end())
            {
                //relay msg to network
                LOG_DEBUG << "ai power provider service on logs req does not have task: " << task_id;
                CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
                
                return E_SUCCESS;
            }

            //get container logs
            const std::string &container_id = it->second->container_id;

            std::shared_ptr<container_logs_req> container_req = std::make_shared<container_logs_req>();
            container_req->container_id = container_id;
            container_req->head_or_tail = req_content->body.head_or_tail;
            container_req->number_of_lines = req_content->body.number_of_lines;
            container_req->timestamps = true;

            std::string log_content;
            std::shared_ptr<container_logs_resp> container_resp = m_container_client->get_container_log(container_req);
            if (nullptr == container_resp)
            {
                LOG_ERROR << "ai power provider service get container logs error: " << task_id;
            }

            //response content
            std::shared_ptr<matrix::service_core::logs_resp> rsp_content = std::make_shared<matrix::service_core::logs_resp>();

            //content header
            rsp_content->header.magic = TEST_NET;
            rsp_content->header.msg_name = LOGS_RESP;
            rsp_content->header.__set_nonce(id_generator().generate_nonce());
            rsp_content->header.__set_session_id(req_content->header.session_id);

            //content body
            rsp_content->body.log.peer_node_id = CONF_MANAGER->get_node_id();
            rsp_content->body.log.log_content = (nullptr == container_resp) ? "get log content error" : std::move(format_logs(container_resp->log_content));
            rsp_content->body.log.log_content.substr(0, MAX_LOG_CONTENT_SIZE);

            //resp msg
            std::shared_ptr<message> resp_msg = std::make_shared<message>();
            resp_msg->set_name(LOGS_RESP);
            resp_msg->set_content(std::dynamic_pointer_cast<base>(rsp_content));
            CONNECTION_MANAGER->broadcast_message(resp_msg);

            return E_SUCCESS;
        }

        std::string ai_power_provider_service::format_logs(const std::string  &raw_logs)
        {
            //docker logs has special format with each line of log:
            // 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
            //we should remove it
            //and ends with 0x30 0x0d 0x0a 
            size_t size = raw_logs.size();
            vector<unsigned char> log_vector(size);

            int push_char_count = 0;
            const char *p = raw_logs.c_str();

            for (size_t i = 0; i < size; )
            {
                //0x30 0x0d 0x0a 
                if ((i + 2 < size)
                    && (0x30 == *p)
                    && (0x0d == *(p + 1))
                    && (0x0a == *(p + 2)))
                {
                    break;
                }

                //skip: 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
                if ((i + 7 < size)
                    && ((0x01 == *p) || (0x02 == *p))
                    && (0x00 == *(p + 1))
                    && (0x00 == *(p + 2))
                    && (0x00 == *(p + 3))
                    && (0x00 == *(p + 4))
                    && (0x00 == *(p + 5))
                    && (0x00 == *(p + 6)))
                {
                    i += 8;
                    p += 8;
                    continue;
                }

                log_vector[push_char_count++] = *p++;
                //p++;
                //push_char_count++;
                i++;
            }

            std::string formatted_str;
            formatted_str.reserve(push_char_count);

            int i = 0;
            while (i < push_char_count)
            {
                formatted_str += log_vector[i++];
            }

            return formatted_str;
        }

        int32_t ai_power_provider_service::on_training_task_timer(std::shared_ptr<core_timer> timer)
        {
            if (0 == m_queueing_tasks.size())
            {
                LOG_DEBUG << "ai power provider service training queueing task is empty";
                return E_SUCCESS;
            }

            //check first task in queue
            std::shared_ptr<ai_training_task> task = m_queueing_tasks.front();
            if (task_queueing == task->status)
            {
                LOG_DEBUG << "ai power provider service training start exec ai training task: " << task->task_id << " status: " << to_training_task_status_string(task->status);;
                return start_exec_training_task(task);
            }
            else if (task_running == task->status)
            {
                LOG_DEBUG << "ai power provider service training start check ai training task: " << task->task_id << " status: " << to_training_task_status_string(task->status);;
                return check_training_task_status(task);
            }
            else
            {
                //pop from task queue
                m_queueing_tasks.pop_front();

                LOG_ERROR << "ai power provider service training start exec ai training task: " << task->task_id << " invalid status: " << to_training_task_status_string(task->status);
                return E_DEFAULT;
            }
        }

        std::shared_ptr<nvidia_config> ai_power_provider_service::get_nividia_config_from_cli()
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

            std::shared_ptr<nvidia_config_resp> resp = m_nvidia_client->get_nividia_config();
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

        std::shared_ptr<container_config> ai_power_provider_service::get_container_config(std::shared_ptr<ai_training_task> task, std::shared_ptr<nvidia_config> nv_config)
        {
            if (nullptr == task || nullptr == nv_config)
            {
                LOG_ERROR << "ai power provider service get container config task or nv_config is nullptr";
                return nullptr;
            }

            std::string::size_type pos = std::string::npos;
            std::shared_ptr<container_config> config = std::make_shared<container_config>();

            //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
            //dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py
            //download file + exec training 
            std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
            exec_cmd += AI_TRAINING_TASK_SCRIPT;

            //tensorflow-cpu           
            pos = m_container_image.find("tensorflow-cpu");
            if (std::string::npos != pos)
            {
                config->cmd.push_back(exec_cmd);
                config->cmd.push_back(task->data_dir);
                config->cmd.push_back(task->code_dir);
                config->cmd.push_back(task->entry_file);

                config->image = m_container_image;

                return config;
            }
           
            //tensorflow-gpu
            pos = m_container_image.find("tensorflow-gpu");
            if (std::string::npos != pos)
            {
                config->cmd.push_back(exec_cmd);
                config->cmd.push_back(task->data_dir);
                config->cmd.push_back(task->code_dir);
                config->cmd.push_back(task->entry_file);

                config->image = m_container_image;

                //env
                config->env.push_back(AI_TRAINING_ENV_PATH);
                config->env.push_back(AI_TRAINING_LD_LIBRARY_PATH);
                config->env.push_back(AI_TRAINING_NVIDIA_VISIBLE_DEVICES);
                config->env.push_back(AI_TRAINING_NVIDIA_DRIVER_CAPABILITIES);
                config->env.push_back(AI_TRAINING_LIBRARY_PATH);

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

                return config;
            }

            return nullptr;
        }

        int32_t ai_power_provider_service::start_exec_training_task(std::shared_ptr<ai_training_task> task)
        {
            assert(nullptr != task);

            //judge retry times
            if (task->retry_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                task->status = task_abnormally_closed;

                //flush to db
                write_task_to_db(task);

                //pop from task queue
                m_queueing_tasks.pop_front();

                LOG_DEBUG << "ai power provider service restart container too many times and close task, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_DEFAULT;
            }

            //first time or contain id empty -> create container
            if (0 == task->retry_times || task->container_id.empty())
            {
                //nv config
                std::shared_ptr<nvidia_config> nv_config = get_nividia_config_from_cli();
                if (nullptr == nv_config)
                {
                    LOG_ERROR << "ai power provider service get nv config error";
                    return E_DEFAULT;
                }

                //container config
                std::shared_ptr<container_config> config = get_container_config(task, nv_config);
                if (nullptr == nv_config)
                {
                    LOG_ERROR << "ai power provider service get container config error";
                    return E_DEFAULT;
                }

                //create container
                task->retry_times++;
                std::shared_ptr<container_create_resp> resp = m_container_client->create_container(config);
                if (nullptr == resp || resp->container_id.empty())
                {
                    //flush to db: update container id, check point 1
                    write_task_to_db(task);

                    LOG_ERROR << "ai power provider service create container error";
                    return E_DEFAULT;
                }
                else
                {
                    task->retry_times = 0;

                    //update container id
                    task->container_id = resp->container_id;

                    //flush to db: update container id, check point 1
                    write_task_to_db(task);

                    LOG_DEBUG << "ai power provider service create container successfully, container id: " << task->container_id;
                }
            }

            //start container
            task->retry_times++;
            int32_t ret = m_container_client->start_container(task->container_id);
            if (E_SUCCESS != ret)
            {
                //flush to db: update status, check point 2
                write_task_to_db(task);

                LOG_ERROR << "ai power provider service start container error, task id: " << task->task_id << "  container id: " << task->container_id;
                return E_DEFAULT;
            }
            else
            {
                //update status
                task->status = task_running;

                //flush to db: update status, check point 2
                write_task_to_db(task);

                LOG_DEBUG << "ai power provider service start container successfully and update task status running, task id: " << task->task_id << "  container id: " << task->container_id;
                return E_SUCCESS;
            }
        }

        int32_t ai_power_provider_service::check_training_task_status(std::shared_ptr<ai_training_task> task)
        {
            //judge retry times
            if (task->retry_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                task->status = task_abnormally_closed;

                //flush to db
                write_task_to_db(task);

                //pop from task queue
                m_queueing_tasks.pop_front();

                LOG_DEBUG << "ai power provider service restart container too many times and close task, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_DEFAULT;
            }

            //inspect container
            std::shared_ptr<container_inspect_response> resp = m_container_client->inspect_container(task->container_id);
            if (nullptr == resp)
            {
                task->retry_times++;
                LOG_ERROR << "ai power provider service check container error, container id: " << task->container_id;
                return E_DEFAULT;
            }

            if (true == resp->state.running)
            {
                LOG_DEBUG << "ai power provider service check container is running, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_SUCCESS;
            }
            else if (0 != resp->state.exit_code)
            {
                LOG_DEBUG << "ai power provider service restart container while inspect container not running, " << "task id: " << task->task_id << " container id: " << task->container_id;

                //restart container
                task->retry_times++;
                start_exec_training_task(task);
                return E_SUCCESS;
            }
            else
            {
                task->status = task_succefully_closed;
                LOG_DEBUG << "ai power provider service restart container while inspect container closed, " << "task id: " << task->task_id << " container id: " << task->container_id;

                //flush to db
                write_task_to_db(task);

                //pop from task queue
                m_queueing_tasks.pop_front();

                return E_SUCCESS;
            }
        }

        int32_t ai_power_provider_service::write_task_to_db(std::shared_ptr<ai_training_task> task)
        {
            assert(nullptr != m_prov_training_task_db && nullptr != task);

            //serialization
            std::shared_ptr<byte_buf> out_buf(new byte_buf);
            binary_protocol proto(out_buf.get());
            task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_prov_training_task_db->Put(write_options, task->task_id, slice);

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::load_task_from_db()
        {
            try
            {
                std::shared_ptr<ai_training_task> task;

                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_prov_training_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    task = std::make_shared<ai_training_task>();

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf(new byte_buf);
                    task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception
                    binary_protocol proto(task_buf.get());
                    task->read(&proto);             //may exception

                    if (0 != m_training_tasks.count(task->task_id))
                    {
                        LOG_ERROR << "ai power provider service training task duplicated: " << task->task_id;
                        continue;
                    }

                    //insert training task
                    m_training_tasks.insert({ task->task_id, task });
                    LOG_DEBUG << "ai power provider service insert ai training task to task map, task id: " << task->task_id << " container_id: " << task->container_id << " task status: " << task->status;

                    //go next
                    if (task_running == task->status || task_queueing == task->status)
                    {
                        //add to queue
                        m_queueing_tasks.push_back(task);
                        LOG_DEBUG << "ai power provider service insert ai training task to task queue, task id: " << task->task_id << " container_id: " << task->container_id << " task status: " << task->status;
                    }
                }

                //sort by received_time_stamp
                m_queueing_tasks.sort(task_time_stamp_comparator());
            }
            catch (...)
            {
                LOG_ERROR << "ai power provider service load task from db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

    }

}