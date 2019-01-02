/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   idle_task_scheduling.cpp
* description    :   idle_task_scheduling for implementation
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
        idle_task_scheduling::idle_task_scheduling(std::shared_ptr<container_worker> & container_worker_ptr) :task_scheduling(container_worker_ptr)
        {
        }

        int32_t idle_task_scheduling::init()
        {
            init_db("idle_task.db");
            return E_SUCCESS;
        }

        int32_t idle_task_scheduling::load_task()
        {
            try
            {
                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_task_db->NewIterator(leveldb::ReadOptions()));
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
                    
                    if (! m_idle_task->training_engine.empty()
                        && E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(m_idle_task->training_engine))
                    {
                        start_pull_image(m_idle_task);
                    }
                    m_idle_task->__set_status(task_stopped);
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

        int32_t idle_task_scheduling::exec_task()
        {
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }

            if (0 == m_idle_state_begin)
            {
                m_idle_state_begin = time_util::get_time_stamp_ms();
                return E_SUCCESS;
            }

            //if dbc is in the idle state more  3 min, the call idle task.
            if (time_util::get_time_stamp_ms() - m_idle_state_begin > m_max_idle_state_interval)
            {
                start_task(m_idle_task);
            }

            return E_SUCCESS;
        }

        int32_t idle_task_scheduling::stop_task()
        {
            m_idle_state_begin = 0;
            if (nullptr == m_idle_task)
            {
                return E_SUCCESS;
            }
            
            m_idle_task->__set_status(task_stopped);
            return task_scheduling::stop_task(m_idle_task);
        }

        void idle_task_scheduling::set_task(std::shared_ptr<idle_task_resp> task)
        {
            if (nullptr == task)
            {
                return;
            }

            if (nullptr == m_idle_task)
            {
                m_idle_task = std::make_shared<ai_training_task>();
            }

            if (CLEAR_IDLE_TASK == task->idle_task_id && !m_idle_task->task_id.empty())
            {
                delete_task(m_idle_task);
                m_idle_task.reset();
            }
            else if (m_idle_task->task_id != task->idle_task_id
                || m_idle_task->hyper_parameters != task->hyper_parameters
                || m_idle_task->code_dir != task->code_dir
                || m_idle_task->training_engine != task->training_engine
                || m_idle_task->data_dir != task->data_dir)
            {
                //update local idle task
                delete_task(m_idle_task);
                LOG_INFO << "update local idle task:" << task->idle_task_id;
                m_idle_task->__set_task_id(task->idle_task_id);
                m_idle_task->__set_code_dir(task->code_dir);
                m_idle_task->__set_entry_file(task->entry_file);
                m_idle_task->__set_training_engine(task->training_engine);
                m_idle_task->__set_data_dir(task->data_dir);
                m_idle_task->__set_hyper_parameters(task->hyper_parameters);

                if (E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(task->training_engine))
                {
                    start_pull_image(m_idle_task);
                }

                write_task_to_db(m_idle_task);
            }
        }
        
        void idle_task_scheduling::update_idle_task()
        {
            set_task(m_fetch_task_handler());
        }

        int8_t idle_task_scheduling::get_status()
        {   //if task is running, return task_running=4; else return task_stopped=8
            if (nullptr == m_idle_task)
            {
                return task_stopped;
            }

            if (m_idle_task->status != task_running)
            {
                return task_stopped;
            }

            return task_running;
        }

        std::string idle_task_scheduling::get_task_id()
        {
            if (nullptr == m_idle_task)
            {
                return "";
            }

            return m_idle_task->task_id;
        }
    }

}