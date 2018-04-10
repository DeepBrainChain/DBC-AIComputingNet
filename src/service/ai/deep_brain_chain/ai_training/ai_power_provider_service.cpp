/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºai_power_provider_service.cpp
* description    £ºai_power_provider_service
* date                  : 2018.01.28
* author            £ºBruce Feng
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



using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;


namespace ai
{
    namespace dbc
    {

		ai_power_provider_service::ai_power_provider_service()
            : m_training_task_timer_id(INVALID_TIMER_ID)
        {

        }

        int32_t ai_power_provider_service::init_conf()
        {
            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            init_subscription();

            init_invoker();

            ret = init_db();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power service init db error";
                return ret;
            }

            ret = init_timer();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power service init timer error";
                return ret;
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::service_exit()
        {
            remove_timer(m_training_task_timer_id);
            return E_SUCCESS;
        }

        void ai_power_provider_service::init_timer_invoker()
        {
            m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&ai_power_provider_service::on_training_task_timer, this, std::placeholders::_1);
        }

        int32_t ai_power_provider_service::on_time_out(std::shared_ptr<core_timer> timer)
        {
            assert(nullptr != timer);

            if (timer->get_name() == AI_TRAINING_TASK_TIMER)
            {
                return on_training_task_timer(timer);
            }

            return E_SUCCESS;

        }

        void ai_power_provider_service::init_subscription()
        {
            TOPIC_MANAGER->subscribe(AI_TRAINGING_NOTIFICATION_RESP, [this](std::shared_ptr<message> &msg) {return send(msg); });
        }

        void ai_power_provider_service::init_invoker()
        {
            invoker_type invoker;

			invoker = std::bind(&ai_power_provider_service::on_start_training_req, this, std::placeholders::_1);
			m_invokers.insert({ AI_TRAINING_NOTIFICATION_REQ,{ invoker } });
			
		    invoker = std::bind(&ai_power_provider_service::on_start_training_resp, this, std::placeholders::_1);
            m_invokers.insert({ AI_TRAINGING_NOTIFICATION_RESP,{ invoker } });

        }

        int32_t ai_power_provider_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;

            //get db path
            fs::path task_db_path = env_manager::get_db_path();
            task_db_path /= fs::path("training_task.db");
            LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

            //open db
            leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
            if (false == status.ok())
            {
                LOG_ERROR << "ai_power_service init training task db error";
                return E_DEFAULT;
            }

            //smart point auto close db
            m_training_task_db.reset(db);

            //load task
            load_task_from_db();
            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::init_timer()
        {
            m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER, AI_TRAINING_TASK_TIMER_INTERVAL);
            if (INVALID_TIMER_ID == m_training_task_timer_id)
            {
                LOG_ERROR << "ai power service add training task timer error";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_start_training_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<matrix::service_core::start_training_req> req = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(content);
            //verify arguments
            //

            //start docker option
            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_start_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<start_training_req> req = std::dynamic_pointer_cast<start_training_req>(msg->get_content());
            assert(nullptr != req);

            //relay start training in network
            LOG_DEBUG << "ai power service relay broadcast start training req to neighbour peer nodes" << req->body.task_id;
            CONNECTION_MANAGER->broadcast_message(msg);

            //check node id
            const std::vector<std::string> &peer_nodes = req->body.peer_nodes_list;
            auto it = peer_nodes.begin();
            for (; it != peer_nodes.end(); it++)
            {
                if ((*it) == CONF_MANAGER->get_node_id())
                {
                    LOG_DEBUG << "ai power service found self node id in on start training: " << req->body.task_id << " node id: " << (*it);
                    break;
                }
            }

            //not found self just return
            if (it == peer_nodes.end())
            {
                LOG_DEBUG << "ai power service found start training req " << req->body.task_id << " is not self and exit function";
                return E_SUCCESS;
            }

            if (nullptr == m_training_task_db)
            {
                LOG_ERROR << "ai power service training task db is nullptr";
                return E_DEFAULT;
            }

            //check task id exists 
            std::string task_value;
            leveldb::Status status = m_training_task_db->Get(leveldb::ReadOptions(), req->body.task_id, &task_value);
            if (status.ok())                //already exists and directly return
            {
                LOG_DEBUG << "ai power service on start training already had task: " << req->body.task_id;
                return E_SUCCESS;
            }

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

            //add to task queue
            m_queueing_tasks.push_back(task);
            assert(0 == m_training_tasks.count(task->task_id));             //memory should be consistent with db
            m_training_tasks[task->task_id] = task;

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::on_training_task_timer(std::shared_ptr<core_timer> timer)
        {
            if (0 == m_queueing_tasks.size())
            {
                LOG_DEBUG << "ai power service training queueing task is empty";
                return E_SUCCESS;
            }

            //check first task in queue
            std::shared_ptr<ai_training_task> task = m_queueing_tasks.front();
            if (task_queueing == task->status)
            {
                LOG_DEBUG << "ai power service training start exec ai training task: " << task->task_id;
                return start_exec_training_task(task);
            }
            else if (task_running == task->status)
            {
                LOG_DEBUG << "ai power service training start check ai training task status" << task->task_id;
                return check_training_task_status(task);
            }
            else
            {
                LOG_ERROR << "ai power service training start exec ai training task: " << task->task_id << " invalid status: " << task->status;
                return E_DEFAULT;
            }
        }

        int32_t ai_power_provider_service::start_exec_training_task(std::shared_ptr<ai_training_task> task)
        {
            //judge retry times
            if (task->retry_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                task->status = task_abnormally_closed;

                //flush to db
                write_task_to_db(task);

                //pop from task queue
                m_queueing_tasks.pop_front();

                LOG_DEBUG << "ai power service restart container too many times, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_DEFAULT;
            }

            //create container
            std::shared_ptr<container_config> config = std::make_shared<container_config>();

            //exec cmd
            std::string exec_cmd = "python ";
            exec_cmd += AI_TRAINING_PYTHON_SCRIPT;
            exec_cmd += AI_TRAINING_PYTHON_SCRIPT_OPTION;
            exec_cmd += task->entry_file;

            std::list<std::string> &cmd = config->cmd;
            cmd.push_back(exec_cmd);                                    //download file + exec training 
            config->image = AI_TRAINING_IMAGE_NAME;
            
            //create container
            std::shared_ptr<container_create_resp> resp = m_container_client->create_container(config);
            if (nullptr == resp || resp->container_id.empty())
            {
                task->retry_times++;
                LOG_ERROR << "ai power  service create container error";
                return E_DEFAULT;
            }

            task->container_id = resp->container_id;
            LOG_DEBUG << "ai power  service create container successfully, container id: " << task->container_id;

            //start container
            int32_t ret = m_container_client->start_container(task->container_id);
            if (E_SUCCESS != ret)
            {
                task->retry_times++;
                LOG_ERROR << "ai power  service start container error, container id: " << task->container_id;
                return E_DEFAULT;
            }

            //update status and container id
            task->status = task_running;

            //flush to db
            write_task_to_db(task);

            LOG_DEBUG << "ai power  service start container update task " << task->task_id << " status runing, container id " << task->container_id << " and flush to db successfully";
            return E_SUCCESS;
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

                LOG_DEBUG << "ai power service restart container too many times, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_DEFAULT;
            }

            //inspect container
            std::shared_ptr<container_inspect_response> resp = m_container_client->inspect_container(task->container_id);
            if (nullptr == resp)
            {
                task->retry_times++;
                LOG_ERROR << "ai power  service check container error, container id: " << task->container_id;
                return E_DEFAULT;
            }
           
            if (true == resp->state.running)
            {
                return E_SUCCESS;
            }
            else if (0 != resp->state.exit_code)
            {
                LOG_DEBUG << "ai power service restart container while inspect container not running, " << "task id: " << task->task_id << " container id: " << task->container_id;

                //restart container
                task->retry_times++;
                start_exec_training_task(task);
                return E_SUCCESS;
            }
            else
            {
                //
            }

            task->status = task_succefully_closed;
            LOG_DEBUG << "ai power service restart container while inspect container closed, " << "task id: " << task->task_id << " container id: " << task->container_id;
           
            //flush to db
            write_task_to_db(task);

            //pop from task queue
            m_queueing_tasks.pop_front();

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::write_task_to_db(std::shared_ptr<ai_training_task> task)
        {
            assert(nullptr != m_training_task_db && nullptr != task);

            //serialization
            std::shared_ptr<byte_buf> out_buf(new byte_buf);
            binary_protocol proto(out_buf.get());
            task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_training_task_db->Put(write_options, task->task_id, slice);

            return E_SUCCESS;
        }

        int32_t ai_power_provider_service::load_task_from_db()
        {
            try
            {
                std::shared_ptr<ai_training_task> task;

                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_training_task_db->NewIterator(leveldb::ReadOptions()));
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
                        LOG_ERROR << "ai power service trainning task duplicated: " << task->task_id;
                        continue;
                    }

                    //insert training task
                    m_training_tasks.insert({task->task_id, task});

                    //go next
                    if (task_running == task->status || task_queueing == task->status)
                    {
                        //add to queue
                        m_queueing_tasks.push_back(task);
                    }
                }

                //sort by received_time_stamp
                m_queueing_tasks.sort(task_time_stamp_comparator());
            }
            catch (...)
            {
                LOG_ERROR << "ai power service load task from db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

    }
}