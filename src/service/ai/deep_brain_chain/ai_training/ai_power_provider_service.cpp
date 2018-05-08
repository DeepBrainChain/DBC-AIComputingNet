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



using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
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
            , m_training_task_timer_id(INVALID_TIMER_ID)
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
            TOPIC_MANAGER->subscribe(AI_TRAINING_NOTIFICATION_REQ, [this](std::shared_ptr<message> &msg) {return send(msg); });

            //stop training
            TOPIC_MANAGER->subscribe(STOP_TRAINING_REQ, [this](std::shared_ptr<message> &msg) {return send(msg); });

            //list training req
            TOPIC_MANAGER->subscribe(LIST_TRAINING_REQ, [this](std::shared_ptr<message> &msg) {return send(msg); });
        }

        void ai_power_provider_service::init_invoker()
        {
            invoker_type invoker;

            invoker = std::bind(&ai_power_provider_service::on_start_training_req, this, std::placeholders::_1);
            m_invokers.insert({ AI_TRAINING_NOTIFICATION_REQ,{ invoker } });

            //stop training
            invoker = std::bind(&ai_power_provider_service::on_stop_training_req, this, std::placeholders::_1);
            m_invokers.insert({ STOP_TRAINING_REQ,{ invoker } });

            //list training req
            invoker = std::bind(&ai_power_provider_service::on_list_training_req, this, std::placeholders::_1);
            m_invokers.insert({ LIST_TRAINING_REQ,{ invoker } });

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
            std::shared_ptr<start_training_req> req = std::dynamic_pointer_cast<start_training_req>(msg->get_content());
            assert(nullptr != req);

            //relay start training in network
            LOG_DEBUG << "ai power provider service relay broadcast start training req to neighbor peer nodes: " << req->body.task_id;
            CONNECTION_MANAGER->broadcast_message(msg);

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
            std::shared_ptr<stop_training_req> req = std::dynamic_pointer_cast<stop_training_req>(msg->get_content());
            assert(nullptr != req);

            //relay on stop_training to network(maybe task running on multiple nodes)
            LOG_DEBUG << "ai power provider service relay broadcast stop_training req to neighbor peer nodes: " << req->body.task_id;
            CONNECTION_MANAGER->broadcast_message(msg);

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
            CONNECTION_MANAGER->broadcast_message(msg);

            if (0 == m_training_tasks.size())
            {
                LOG_DEBUG << "ai power provider service training queuing task is empty";
                return E_SUCCESS;
            }

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::make_shared<matrix::service_core::list_training_resp>();

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
                //create container
                std::shared_ptr<container_config> config = std::make_shared<container_config>();

                //exec cmd: dbc_task.sh data_dir_hash code_dir_hash ai_training_python
                ///dbc_task.sh Qme2UKa6yi9obw6MUcCRbpZBUmqMnGnznti4Rnzba5BQE3 QmbA8ThUawkUNtoV7yjso6V8B1TYeCgpXDhMAfYCekTNkr ai_training.py
                std::string exec_cmd = AI_TRAINING_TASK_SCRIPT_HOME;
                exec_cmd += AI_TRAINING_TASK_SCRIPT;
                //exec_cmd += " ";
                //exec_cmd += task->data_dir;
                //exec_cmd += " ";
                //exec_cmd += task->code_dir;
                //exec_cmd += " ";
                //exec_cmd += task->entry_file;

                config->cmd.push_back(exec_cmd);                                    //download file + exec training 
                config->cmd.push_back(task->data_dir);
                config->cmd.push_back(task->code_dir);
                config->cmd.push_back(task->entry_file);
                config->image = m_container_image;

                //create container
                task->retry_times++;
                std::shared_ptr<container_create_resp> resp = m_container_client->create_container(config);
                if (nullptr == resp || resp->container_id.empty())
                {
                    //flush to db: update container id, checki point 1
                    write_task_to_db(task);

                    LOG_ERROR << "ai power provider service create container error";
                    return E_DEFAULT;
                }
                else
                {
                    task->retry_times = 0;

                    //update container id
                    task->container_id = resp->container_id;

                    //flush to db: update container id, checki point 1
                    write_task_to_db(task);

                    LOG_DEBUG << "ai power provider service create container successfully, container id: " << task->container_id;
                }
            }

            //start container
            task->retry_times++;
            int32_t ret = m_container_client->start_container(task->container_id);
            if (E_SUCCESS != ret)
            {
                //flush to db: update status, checki point 2
                write_task_to_db(task);

                LOG_ERROR << "ai power provider service start container error, task id: " << task->task_id << "  container id: " << task->container_id;
                return E_DEFAULT;
            }
            else
            {
                //update status
                task->status = task_running;

                //flush to db: update status, checki point 2
                write_task_to_db(task);

                LOG_DEBUG << "ai power provider service start container successfully and update task status runing, task id: " << task->task_id << "  container id: " << task->container_id;
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