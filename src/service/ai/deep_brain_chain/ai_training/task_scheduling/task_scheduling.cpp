/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   oss_task_manager.cpp
* description    :   oss_task_manager for implementation
* date                  :   2018.10.17
* author            :   Regulus
**********************************************************************************/
#include "common.h"
#include "task_scheduling.h"
#include "util.h"
#include "utilstrencodings.h"
#include "task_common_def.h"
#include "server.h"
#include <boost/format.hpp>
#include "time_util.h"
#include "url_validator.h"

namespace ai
{
    namespace dbc
    {
        task_scheduling::task_scheduling(std::shared_ptr<container_worker> container_worker_ptr)
        {
            m_container_worker = container_worker_ptr;
        }
        
        int32_t task_scheduling::init_db(std::string db_path)
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;
            try
            {
                //get db path
                fs::path task_db_path = env_manager::get_db_path();
                if (false == fs::exists(task_db_path))
                {
                    LOG_DEBUG << "db directory path does not exist and create db directory" << task_db_path;
                    fs::create_directory(task_db_path);
                }

                //check db directory
                if (false == fs::is_directory(task_db_path))
                {
                    LOG_ERROR << "db directory path does not exist and exit" << task_db_path;
                    return E_DEFAULT;
                }

                task_db_path /= fs::path(db_path);
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
                if (false == status.ok())
                {
                    LOG_ERROR << "init idle task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_task_db.reset(db);

                //load task
                load_task();

                LOG_INFO << "idle task db path:" << task_db_path;
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create idle task db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create idle task db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t task_scheduling::write_task_to_db(std::shared_ptr<ai_training_task> task)
        {
            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_task_db->Put(write_options, task->task_id, slice);

            return E_SUCCESS;
        }

        int32_t task_scheduling::create_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }

            if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty())
            {
                LOG_DEBUG << "idle task config error.";
                return E_DEFAULT;
            }

            std::shared_ptr<container_config> config = m_container_worker->get_container_config(task);
            std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task->task_id);
            if (resp != nullptr && !resp->container_id.empty())
            {
                task->__set_container_id(resp->container_id);
                LOG_DEBUG << "create idle task success. idle task id:" << task->task_id << " container id:" << task->container_id;

                return E_SUCCESS;
            }
            else
            {
                LOG_ERROR << "create idle task failed. idle task id:" << task->task_id;
            }
            return E_DEFAULT;
        }

        int32_t task_scheduling::start_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_SUCCESS;
            }

            if (DBC_TASK_RUNNING == get_task_state(task))
            {
                LOG_DEBUG << "idle task have been running, do not need to start. Idle task id:" << task->task_id;
                return E_SUCCESS;
            }

            //if image do not exist, then pull it
            if (E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(task->training_engine))
            {
                return start_pull_image(task);
            }

            if (task->container_id.empty())
            {
                //if container_id does not exist, means dbc need to create container
                if (E_SUCCESS != create_task(task))
                {
                    LOG_ERROR << "create idle task error";
                    return E_DEFAULT;
                }
            }

            int32_t ret = CONTAINER_WORKER_IF->start_container(task->container_id);

            if (ret != E_SUCCESS)
            {
                LOG_ERROR << "Start task error. Task id:" << task->task_id;
                return E_DEFAULT;
            }

            LOG_INFO << "start task success. Task id:" << task->task_id;
            task->__set_start_time(time_util::get_time_stamp_ms());
            task->status = task_running;
            task->error_times = 0;
            write_task_to_db(task);
            return E_SUCCESS;
        }

        int32_t task_scheduling::stop_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task || task->container_id.empty())
            {
                return E_SUCCESS;
            }
            LOG_INFO << "stop idle task " << task->task_id;
            task->__set_end_time(time_util::get_time_stamp_ms());
            write_task_to_db(task);
            return CONTAINER_WORKER_IF->stop_container(task->container_id);
        }

        int32_t task_scheduling::delete_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_SUCCESS;
            }

            try
            {
                if (DBC_TASK_RUNNING == get_task_state(task))
                {
                    stop_task(task);
                }

                if (!task->container_id.empty())
                {
                    CONTAINER_WORKER_IF->remove_container(task->container_id);
                }
                LOG_INFO << "delete task " << task->task_id;
                m_task_db->Delete(leveldb::WriteOptions(), task->task_id);
            }
            catch (...)
            {
                LOG_ERROR << "delete task abnormal";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        TASK_STATE task_scheduling::get_task_state(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task || task->task_id.empty())
            {
                return DBC_TASK_NULL;
            }
            //inspect container
            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->task_id);
            if (nullptr == resp)
            {
                task->__set_container_id("");
                return DBC_TASK_NOEXIST;
            }
            
            //local db may be deleted, but idle_task is running, then container_id is empty.
            if (!resp->id.empty() && task->container_id.empty())
            {
                task->__set_container_id(resp->id);
            }

            if (true == resp->state.running)
            {
                return DBC_TASK_RUNNING;
            }

            return DBC_TASK_STOPPED;
        }

        int32_t task_scheduling::start_pull_image(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task || task->training_engine.empty())
            {
                return E_SUCCESS;
            }

            //check local evn.
            auto ret = m_container_worker->can_pull_image();
            if (E_SUCCESS != m_container_worker->can_pull_image())
            {
                return ret;
            }

            if (nullptr == m_pull_image_mng)
            {
                m_pull_image_mng = std::make_shared<image_manager>();
            }

            //if the idle task pulling image is not the idle task need image.  
            if (!m_pull_image_mng->get_pull_image_name().empty()
                && m_pull_image_mng->get_pull_image_name() != task->training_engine)
            {
                if (PULLING == m_pull_image_mng->check_pull_state())
                {
                    m_pull_image_mng->terminate_pull();
                }
            }

            //if training_engine is pulling, then return.
            if (PULLING == m_pull_image_mng->check_pull_state())
            {
                return E_SUCCESS;
            }

            //start pulling
            if (E_SUCCESS != m_pull_image_mng->start_pull(task->training_engine))
            {
                LOG_ERROR << "idle task engine pull fail. engine:" << task->training_engine;
                return E_PULLING_IMAGE;
            }

            m_pull_image_mng->set_start_time(time_util::get_time_stamp_ms());

            if (task_queueing == task->status)
            {
                task->__set_status(task_pulling_image);
                LOG_DEBUG << "docker pulling image. change status to " << to_training_task_status_string(task->status)
                    << " task id:" << task->task_id << " image engine:" << task->training_engine;
                write_task_to_db(task);
            }

            return E_SUCCESS;
        }

        int32_t task_scheduling::stop_pull_image(std::shared_ptr<ai_training_task> task)
        {
            if (!m_pull_image_mng)
            {
                return E_SUCCESS;
            }

            if (task->training_engine != m_pull_image_mng->get_pull_image_name())
            {
                LOG_ERROR << "pull image is not " << task->training_engine;
                return E_SUCCESS;
            }

            if (nullptr != m_pull_image_mng)
            {
                LOG_INFO << "terminate pull " << task->training_engine;
                m_pull_image_mng->terminate_pull();
            }

            return E_SUCCESS;
        }
    }
}