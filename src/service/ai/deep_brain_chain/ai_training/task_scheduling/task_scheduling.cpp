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


        int32_t task_scheduling::init_db(std::string db_name)
        {
            auto rtn = m_task_db.init_db(env_manager::get_db_path(),db_name);
            if (E_SUCCESS != rtn)
            {
                return rtn;
            }

            try
            {
                load_task();
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "load task from db error: " << e.what();
                return E_DEFAULT;
            }

            return E_SUCCESS;

        }


        int32_t task_scheduling::update_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }
            if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty())
            {
                LOG_DEBUG << "task config error.";
                return E_DEFAULT;
            }

          //  auto state = get_task_state(task);

                if (task->container_id.empty())
                {

                    LOG_INFO << "container_id null. container_id";

                    return E_DEFAULT;
                }

                std::shared_ptr<update_container_config> config = m_container_worker->get_update_container_config(task);
                int32_t ret= CONTAINER_WORKER_IF->update_container(task->container_id, config);


                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "update task error. Task id:" << task->task_id;
                    return E_DEFAULT;
                }

                LOG_INFO << "update task success. Task id:" << task->task_id;
//                task->__set_update_time(time_util::get_time_stamp_ms());
                task->__set_start_time(time_util::get_time_stamp_ms());
                task->__set_status(task_running);
                task->error_times = 0;

   //             task->__set_memory(config->memory);
   //             task->__set_memory_swap(config->memory_swap);
               // task->__set_gpus(config->env);
                m_task_db.write_task_to_db(task);







            return E_SUCCESS;
        }

        int32_t task_scheduling::commit_image(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }

            if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty())
            {
                LOG_DEBUG << "task config error.";
                return E_DEFAULT;
            }

            std::shared_ptr<container_config> config = m_container_worker->get_container_config(task);
            std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task->task_id);
            if (resp != nullptr && !resp->container_id.empty())
            {
                task->__set_container_id(resp->container_id);
                LOG_DEBUG << "create task success. task id:" << task->task_id << " container id:" << task->container_id;

                return E_SUCCESS;
            }
            else
            {
                LOG_ERROR << "create task failed. task id:" << task->task_id;
            }
            return E_DEFAULT;
        }


        int32_t task_scheduling::create_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }

            if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty())
            {
                LOG_INFO << "task config error.";
                return E_DEFAULT;
            }


            std::shared_ptr<container_config> config = m_container_worker->get_container_config(task);
            std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task->task_id);
            if (resp != nullptr && !resp->container_id.empty())
            {
                task->__set_container_id(resp->container_id);
                LOG_INFO << "create task success. task id:" << task->task_id << " container id:" << task->container_id;

                return E_SUCCESS;
            }
            else
            {
                LOG_ERROR << "create task failed. task id:" << task->task_id;
            }
            return E_DEFAULT;
        }

        int32_t task_scheduling::start_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_SUCCESS;
            }




            auto state = get_task_state(task);
            if (DBC_TASK_RUNNING == state)
            {
                if (task->status != task_running)
                {
                    task->__set_status(task_running);
                    m_task_db.write_task_to_db(task);
                }
                LOG_DEBUG << "task have been running, do not need to start. task id:" << task->task_id;
                return E_SUCCESS;
            }

            //if image do not exist, then pull it
            if (E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(task->training_engine))
            {
                return start_pull_image(task);
            }

            bool is_container_existed = (!task->container_id.empty());
            if (!is_container_existed)
            {
                //if container_id does not exist, means dbc need to create container
                if (E_SUCCESS != create_task(task))
                {
                    LOG_ERROR << "create task error";
                    return E_DEFAULT;
                }
            }


            // update container's parameter if
            std::string path = env_manager::get_home_path().generic_string() + "/container/parameters";
            std::string text = "task_id=" + task->task_id + "\n";

            LOG_INFO << " container_id: " << task->container_id << " task_id: " << task->task_id;

            if (is_container_existed)
            {
                // server_specification indicates the container to be reused for this task
                // needs to indicate container run with different parameters
                text += ("code_dir=" + task->code_dir + "\n");

                if (task->server_specification == "restart")
                {
                    // use case: restart a task
                    text += ("restart=true\n");
                }
            }


            if (!file_util::write_file(path, text))
            {
                LOG_ERROR << "fail to refresh task's code_dir before reusing existing container for new task "
                          << task->task_id;
                return E_DEFAULT;
            }


            int32_t ret = CONTAINER_WORKER_IF->start_container(task->container_id);

            if (ret != E_SUCCESS)
            {
                LOG_ERROR << "Start task error. Task id:" << task->task_id;
                return E_DEFAULT;
            }

            LOG_INFO << "start task success. Task id:" << task->task_id;
            task->__set_start_time(time_util::get_time_stamp_ms());
            task->__set_status(task_running);
            task->error_times = 0;
            LOG_INFO << "task status:" << "task_running";
            m_task_db.write_task_to_db(task);
            LOG_INFO << "task status:" << "write_task_to_db";
            LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
            return E_SUCCESS;
        }

        int32_t task_scheduling::stop_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task || task->container_id.empty())
            {
                return E_SUCCESS;
            }
            LOG_INFO << "stop task " << task->task_id;
            task->__set_end_time(time_util::get_time_stamp_ms());
            m_task_db.write_task_to_db(task);
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

                m_task_db.delete_task(task);
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

            // inspect container
            std::string container_id = task->task_id;

            // container can be started again by task delivered latter,
            // in that case, the container's id and name keeps the original value, then new task's id and container's name does not equal any more.
            if(!task->container_id.empty())
            {
                container_id = task->container_id;
            }

            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(container_id);
            if (nullptr == resp)
            {
                task->__set_container_id("");
                return DBC_TASK_NOEXIST;
            }
            
            //local db may be deleted, but task is running, then container_id is empty.
            if (!resp->id.empty() && ( task->container_id.empty() || resp->id != task->container_id))
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

            //if the task pulling image is not the task need image.
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
                LOG_ERROR << "task engine pull fail. engine:" << task->training_engine;
                return E_PULLING_IMAGE;
            }

            m_pull_image_mng->set_start_time(time_util::get_time_stamp_ms());

            if (task_queueing == task->status)
            {
                task->__set_status(task_pulling_image);
                LOG_DEBUG << "docker pulling image. change status to " << to_training_task_status_string(task->status)
                    << " task id:" << task->task_id << " image engine:" << task->training_engine;
                m_task_db.write_task_to_db(task);
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

            LOG_INFO << "terminate pull " << task->training_engine;
            m_pull_image_mng->terminate_pull();

            return E_SUCCESS;
        }
    }
}