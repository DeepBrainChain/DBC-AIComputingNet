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
#include "user_task_scheduling.h"
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
        user_task_scheduling::user_task_scheduling(std::shared_ptr<container_worker> & container_worker_ptr):task_scheduling(container_worker_ptr)
        {
        }
        
        int32_t user_task_scheduling::init()
        {
            init_db("prov_training_task.db");
            if (CONF_MANAGER->get_prune_task_stop_interval() < 1 || CONF_MANAGER->get_prune_task_stop_interval() > 8760)
            {
                return E_DEFAULT;
            }
            m_prune_intervel = CONF_MANAGER->get_prune_task_stop_interval();
            return E_SUCCESS;
        }

        int32_t user_task_scheduling::load_task()
        {
            try
            {
                std::shared_ptr<ai_training_task> task;

                //iterate task in db
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    task = std::make_shared<ai_training_task>();

                    //deserialization
                    std::shared_ptr<byte_buf> task_buf = std::make_shared<byte_buf>();
                    task_buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());            //may exception
                    binary_protocol proto(task_buf.get());
                    task->read(&proto);             //may exception

                    if (0 != m_training_tasks.count(task->task_id))
                    {
                        LOG_ERROR << "user training task duplicated: " << task->task_id;
                        continue;
                    }

                    //insert training task
                    m_training_tasks.insert({ task->task_id, task });
                    LOG_DEBUG << "user task scheduling insert ai training task to task map, task id: " << task->task_id << " container_id: " << task->container_id << " task status: " << to_training_task_status_string(task->status);

                    //go next
                    if (task_running == task->status || task_queueing == task->status || task_pulling_image == task->status)
                    {
                        //add to queue
                        m_queueing_tasks.push_back(task);
                        LOG_DEBUG << "user task scheduling insert ai training task to task queue, task id: " << task->task_id << " container_id: " << task->container_id << " task status: " << to_training_task_status_string(task->status);
                    }
                }

                //sort by received_time_stamp
                m_queueing_tasks.sort(task_time_stamp_comparator());
            }
            catch (...)
            {
                LOG_ERROR << "user task scheduling load task from db exception";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t user_task_scheduling::exec_task()
        {
            auto task = m_queueing_tasks.front();

            //judge retry times
            if (task->error_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                stop_task(task, task_abnormally_closed);

                LOG_WARNING << "ai power provider service restart container too many times and close task, " << "task id: " << task->task_id << " container id: " << task->container_id;
                return E_DEFAULT;
            }

            if (E_SUCCESS == auth_task(task))
            {
                LOG_DEBUG << "training start exec ai training task: " << task->task_id << " status: " << to_training_task_status_string(task->status);;
            }
            else
            {
                LOG_ERROR << "auth failed. " << " drop task:" << task->task_id;
                stop_task(task, task_overdue_closed);
                return E_SUCCESS;
            }

            //user can start training, should stop idle task.
            if (nullptr != m_stop_idle_task_handler)
            {
                m_stop_idle_task_handler();
            }

            int32_t ret = start_task(task);

            if (ret != E_SUCCESS)
            {
                task->error_times++;
                switch (ret)
                {
                case E_NO_DISK_SPACE:
                    stop_task(task, task_nospace_closed);
                    break;
                case E_PULLING_IMAGE:
                case E_IMAGE_NOT_FOUND:
                    stop_task(task, task_noimage_closed);
                    break;
                default:
                    write_task_to_db(task);
                    break;
                }
                
                LOG_ERROR << "start user task failed, task id:" << task->task_id;
                return E_DEFAULT;
            }
            LOG_DEBUG << "start user task success, task id:" << task->task_id;
            return E_SUCCESS;
        }

        int32_t user_task_scheduling::stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status)
        {
            if (m_queueing_tasks.empty())
            {
                return E_SUCCESS;
            }
            auto top_task = m_queueing_tasks.front();
            m_queueing_tasks.remove(task);
            if (task->status >= task_stopped)
            {
                return E_SUCCESS;
            }

            if (task_running == task->status)
            {
                int32_t ret = task_scheduling::stop_task(task);
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "stop container error, container id: " << task->container_id << " task is:" << task->task_id;
                }
                LOG_INFO << "stop container success, container id: " << task->container_id << " task is:" << task->task_id;
            }

            if (task_pulling_image == task->status)
            {
                stop_pull_image(task);
            }
            task->__set_end_time(time_util::get_time_stamp_ms());
            task->__set_status(end_status);
            write_task_to_db(task);
            
            if (end_status != task_overdue_closed)
            {
                //if task is not the top task, means the task is have never been scheduled. 
                //At this time, the task is not needed to report task status to oss..
                if (task->task_id != top_task->task_id)
                {
                    LOG_DEBUG << "task is not the top task, do not need report status to oss. task is: " << task->task_id;
                    return E_SUCCESS;
                }
                LOG_INFO << "dbc close task, report the event to oss system." << " task:" << task->task_id << " status:" << to_training_task_status_string(end_status);
                return auth_task(task);
            }

            return E_SUCCESS;
        }

        void user_task_scheduling::add_task(std::shared_ptr<ai_training_task> task)
        {
            if (m_training_tasks.size() > MAX_TASK_COUNT)
            {
                LOG_ERROR << "task is full.";
                return;
            }
            
            //flush to db
            if (E_SUCCESS != write_task_to_db(task))
            {
                return;
            }
            LOG_INFO << "user task scheduling flush task to db: " << task->task_id;
            m_queueing_tasks.push_back(task);
            m_training_tasks[task->task_id] = task;
        }

        std::shared_ptr<ai_training_task> user_task_scheduling::find_task(std::string task_id)
        { 
            auto it_task = m_training_tasks.find(task_id);
            if (it_task != m_training_tasks.end())
            {
                return it_task->second;
            }

            //this may be never happend
            std::string task_value;
            leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
            if (!status.ok() || status.IsNotFound())
            {
                return nullptr;
            }

            std::shared_ptr<ai_training_task> task_ptr = std::make_shared<ai_training_task>();
            //deserialization
            std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
            buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
            binary_protocol proto(buf.get());
            task_ptr->read(&proto);

            return task_ptr;
        }

        int32_t user_task_scheduling::check_pull_image_state()
        {
            if (m_queueing_tasks.empty())
            {
                return E_SUCCESS;
            }

            auto task = m_queueing_tasks.front();
            if (task->status != task_pulling_image)
            {
                LOG_ERROR << "task state is not task_pulling_image. task id:" << task->task_id;
                return E_DEFAULT;
            }

            if (m_pull_image_mng != nullptr && m_pull_image_mng->get_pull_image_name() == task->training_engine)
            {
                //cost time too long to pull image.
                if ((time_util::get_time_stamp_ms() - m_pull_image_mng->get_start_time()) >= AI_PULLING_IMAGE_TIMER_INTERVAL)
                {
                    //pull error
                    LOG_ERROR << "pull image too long." << " engine image:" << task->training_engine;
                    stop_task(task, task_noimage_closed);
                    return E_PULLING_IMAGE;
                }
                else
                {
                    if (PULLING == m_pull_image_mng->check_pull_state())
                    {
                        if (E_SUCCESS != auth_task(task))
                        {
                            return E_DEFAULT;
                        }
                        LOG_DEBUG << "pulling: docker pull " << task->training_engine;
                        return E_SUCCESS;
                    }
                    else
                    {
                        //docker is not pulling image.
                        if (CONTAINER_WORKER_IF->exist_docker_image(task->training_engine) != E_SUCCESS)
                        {
                            LOG_DEBUG << "docker pull image fail. engine: " << task->training_engine;
                            LOG_WARNING << "ai power provider service pull image failed";
                            stop_task(task, task_noimage_closed);
                            return E_PULLING_IMAGE;
                        }

                        LOG_DEBUG << "docker pull image success. engine: " << task->training_engine;
                        task->__set_status(task_queueing);
                        LOG_DEBUG << "change task status to " << to_training_task_status_string(task->status) << " task id:" << task->task_id;
                    }
                }
            }
            else
            {
                //task state is pulling, but image is not pulling. so dbc may be restarted. 
                task->__set_status(task_queueing);
            }

            return E_SUCCESS;
        }

        int32_t user_task_scheduling::auth_task(std::shared_ptr<ai_training_task> task)
        {
            auto ret = m_auth_task_handler != nullptr ? m_auth_task_handler(task) : E_SUCCESS;

            if (E_SUCCESS != ret && task->status < task_stopped)
            {
                LOG_ERROR << "auth failed. " << "drop task:" << task->task_id;
                stop_task(task, task_overdue_closed);
            }
            return ret;
        }

        int32_t user_task_scheduling::check_training_task_status()
        {
            auto task = m_queueing_tasks.front();
            //judge retry times
            if (task->error_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                LOG_WARNING << "user task restart container too many times and close task, " << "task id: " << task->task_id << " container id: " << task->container_id;
                stop_task(task, task_abnormally_closed);                
                return E_DEFAULT;
            }

            //inspect container
            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->container_id);
            if (nullptr == resp)
            {
                LOG_ERROR << "user task check container error, container id: " << task->container_id;

                task->error_times++;
                //flush to db
                write_task_to_db(task);
                return E_DEFAULT;
            }

            if (true == resp->state.running)
            {
                LOG_DEBUG << "ai power provider service check container is running, " << "task id: " << task->task_id << " container id: " << task->container_id;
                auth_task(task);
                return E_SUCCESS;
            }

            else if (0 != resp->state.exit_code)
            {
                LOG_INFO << "inspect container not running, " << "task id: " << task->task_id << " container id: " << task->container_id << " exit_code" << resp->state.exit_code;
                stop_task(task, task_abnormally_closed);
                return E_SUCCESS;
            }
            else
            {
                LOG_INFO << "user task inspect container success closed, " << "task id: " << task->task_id << " container id: " << task->container_id;
                stop_task(task, task_successfully_closed);
                return E_SUCCESS;
            }
            return E_SUCCESS;
        }

        int32_t user_task_scheduling::process_task()
        {
            if (m_queueing_tasks.empty())
            {
                return E_SUCCESS;
            }

            auto task = m_queueing_tasks.front();
            if (task_queueing == task->status)
            {
                return  exec_task();
            }
            else if (task_pulling_image == task->status)
            {
                return check_pull_image_state();
            }
            else if (task_running == task->status)
            {
                LOG_DEBUG << "training start check ai training task: " << task->task_id << " status: " << to_training_task_status_string(task->status);;
                return check_training_task_status();
            }
            else
            {
                //drop this task.
                m_queueing_tasks.pop_front();
                LOG_ERROR << "training start exec ai training task: " << task->task_id << " invalid status: " << to_training_task_status_string(task->status);
                return E_DEFAULT;
            }
            return E_SUCCESS;
        }

        int32_t user_task_scheduling::prune_task()
        {
            prune_task(m_prune_intervel);

            return E_SUCCESS;
        }

        int32_t user_task_scheduling::prune_task(int16_t interval)
        {
            int64_t cur = time_util::get_time_stamp_ms();

            int64_t p_interval = interval* 3600*1000;

            LOG_INFO << "prune docker container." << " interval:" << interval << "h";

            for (auto task_iter = m_training_tasks.begin(); task_iter != m_training_tasks.end();)
            {
                if (task_iter->second->status < task_stopped)
                {
                    task_iter++;
                    continue;
                }

                //1. if task have been stop too long
                //2. if task have been stopped , and  container_id is empty means task have never been exectue
                if ((cur - task_iter->second->end_time) > p_interval
                    ||task_iter->second->container_id.empty())
                {
                    delete_task_from_db(task_iter->second);
                    m_training_tasks.erase(task_iter++);
                }
                else
                {
                    task_iter++;
                }
            }

            if (0 == interval)
            {
                return E_SUCCESS;
            }

            if (m_training_tasks.size() > MAX_PRUNE_TASK_COUNT)
            {
                prune_task(interval / 2);
            }

            return E_SUCCESS;
        }
    }
}