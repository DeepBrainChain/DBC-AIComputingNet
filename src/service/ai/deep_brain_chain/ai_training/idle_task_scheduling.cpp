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
#include "idle_task_scheduling.h"
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
        idle_task_scheduling::idle_task_scheduling(std::shared_ptr<container_client> & container_client_ptr)
        {
            m_container_client = container_client_ptr;
            init_db();
        }
        
        int32_t idle_task_scheduling::init_db()
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

                task_db_path /= fs::path("idle_task.db");
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
                if (false == status.ok())
                {
                    LOG_ERROR << "init idle task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_idle_task_db.reset(db);

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

        int32_t idle_task_scheduling::write_task_to_db()
        {
            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            m_idle_task->write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            m_idle_task_db->Put(write_options, m_idle_task->task_id, slice);

            return E_SUCCESS;
        }

        int32_t idle_task_scheduling::load_task()
        {
            try
            {
                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_idle_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    if (nullptr == m_idle_task)
                    {
                        m_idle_task = std::make_shared<ai_training_task>();
                    }

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
                    task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception
                    binary_protocol proto(task_buf.get());
                    m_idle_task->read(&proto);            //may exception
                    break;
                }
            }
            catch (...)
            {
                LOG_ERROR << "load idle task from db exception";
                return E_DEFAULT;
            }
            return E_SUCCESS;
        }

        int32_t idle_task_scheduling::create_task(get_idle_task_container_config get_config)
        {
            if (nullptr == m_idle_task)
            {
                return E_DEFAULT;
            }

            if (m_idle_task->entry_file.empty() || m_idle_task->code_dir.empty() || m_idle_task->task_id.empty())
            {
                LOG_DEBUG << "idle task config error.";
                return E_DEFAULT;
            }

            std::shared_ptr<container_config> config = get_config(m_idle_task);
            std::shared_ptr<container_create_resp> resp = m_container_client->create_container(config, m_idle_task->task_id);
            if (resp != nullptr && !resp->container_id.empty())
            {
                m_idle_task->__set_container_id(resp->container_id);
                LOG_DEBUG << "create idle task success. idle task id:" << m_idle_task->task_id << " container id:" << m_idle_task->container_id;

                return E_SUCCESS;
            }
            else
            {
                LOG_ERROR << "create idle task failed. idle task id:" << m_idle_task->task_id;
            }
            return E_DEFAULT;
        }

        int32_t idle_task_scheduling::exec_task(get_idle_task_container_config get_config)
        {
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }

            if (IDLE_TASK_RUNNING == get_task_state())
            {
                LOG_DEBUG << "idle task have been running, do not need to start. Idle task id:" << m_idle_task->task_id;
                return E_SUCCESS;
            }

            //first, set the current time to m_idle_state_begin
            if (0 == m_idle_state_begin)
            {
                m_idle_state_begin = time_util::get_time_stamp_ms();
                return E_SUCCESS;
            }

            //if dbc is in the idle state more  3 min, the call idle task.
            if (time_util::get_time_stamp_ms() - m_idle_state_begin > m_max_idle_state_interval)
            {
                if (m_idle_task->container_id.empty())
                {
                    //if container_id does not exist, means dbc need to create container
                    if (E_SUCCESS != create_task(get_config))
                    {
                        LOG_ERROR << "create idle task error";
                        return E_DEFAULT;
                    }
                }
                
                int32_t ret = m_container_client->start_container(m_idle_task->container_id);
                
                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "start idle task error. Idle task id:" << m_idle_task->task_id;
                    return E_SUCCESS;
                }

                LOG_INFO << "start idle task success. Idle task id:" << m_idle_task->task_id;
                m_idle_task->__set_start_time(time_util::get_time_stamp_ms());
                write_task_to_db();
            }

            return E_SUCCESS;
        }

        int32_t idle_task_scheduling::stop_task()
        {
            m_idle_state_begin = 0;

            if (nullptr == m_idle_task ||  m_idle_task->container_id.empty())
            {
                return E_SUCCESS;
            }
            LOG_INFO << "stop idle task " << m_idle_task->task_id;
            m_idle_task->__set_end_time(time_util::get_time_stamp_ms());
            write_task_to_db();
            return m_container_client->stop_container(m_idle_task->container_id);
        }

        int32_t idle_task_scheduling::delete_task()
        {
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }

            try
            {
                if (IDLE_TASK_RUNNING == get_task_state())
                {
                    stop_task();
                }

                if (!m_idle_task->container_id.empty())
                {
                    m_container_client->remove_container(m_idle_task->container_id);
                }
                LOG_INFO << "delete idle task " << m_idle_task->task_id;
                m_idle_task_db->Delete(leveldb::WriteOptions(), m_idle_task->task_id);
            }
            catch (...)
            {
                LOG_ERROR << "p2p net service clear peer candidates db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        idle_task_state idle_task_scheduling::get_task_state()
        {
            if (nullptr == m_idle_task || m_idle_task->container_id.empty())
            {
                return IDLE_TASK_STOPPED;
            }
            //inspect container
            std::shared_ptr<container_inspect_response> resp = m_container_client->inspect_container(m_idle_task->container_id);
            if (nullptr == resp)
            {
                m_idle_task->__set_container_id("");
                return IDLE_TASK_NOEXIST;
            }

            if (true == resp->state.running)
            {
                return IDLE_TASK_RUNNING;
            }

            if (!resp->id.empty())
            {
                m_idle_task->__set_container_id(resp->id);
            }
            return IDLE_TASK_STOPPED;
        }

        void idle_task_scheduling::set_task(std::shared_ptr<idle_task_resp> task)
        {
            if (nullptr == m_idle_task)
            {
                m_idle_task = std::make_shared<ai_training_task>();
            }
            if (task->idle_task_id == "0" && !m_idle_task->task_id.empty())
            {
                //idle_task_id=="0", means delete local idle task
                delete_task();
                m_idle_task.reset();
            }
            else if (m_idle_task->task_id != task->idle_task_id)
            {
                //update local idle task
                delete_task();
                LOG_INFO << "update local idle task:" << task->idle_task_id;
                m_idle_task->__set_task_id(task->idle_task_id);
                m_idle_task->__set_code_dir(task->code_dir);
                m_idle_task->__set_entry_file(task->entry_file);
                m_idle_task->__set_training_engine(task->training_engine);
                m_idle_task->__set_data_dir(task->data_dir);
                m_idle_task->__set_hyper_parameters(task->hyper_parameters);
                write_task_to_db();
            }
        }

    }

}