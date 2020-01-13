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

#include <stdlib.h>
#include "service_name.h"

namespace ai
{
    namespace dbc
    {
        user_task_scheduling::user_task_scheduling(std::shared_ptr<container_worker> container_worker_ptr)
                : task_scheduling(container_worker_ptr)
        {
        }

        int32_t user_task_scheduling::init(bpo::variables_map &options)
        {
            if (options.count(SERVICE_NAME_AI_TRAINING))
            {
                m_is_computing_node = true;
                //todo: driver not ready yet when dbc starts
//                LOG_INFO << "wait 30 seconds for nvidia gpu initialization completed";
//                std::this_thread::sleep_for(std::chrono::milliseconds(30000));
                gpu_pool_helper::update_gpu_from_proc(m_gpu_pool, "/proc/driver/nvidia/gpus");

                LOG_INFO << m_gpu_pool.toString();

            }


            init_db("prov_training_task.db");
            if (CONF_MANAGER->get_prune_task_stop_interval() < 1 || CONF_MANAGER->get_prune_task_stop_interval() > 8760)
            {
                return E_DEFAULT;
            }
            m_prune_intervel = CONF_MANAGER->get_prune_task_stop_interval();


            return E_SUCCESS;
        }

        void user_task_scheduling::update_gpu_info_from_proc()
        {
            if (m_is_computing_node)
            {
                gpu_pool gp;
                gpu_pool_helper::update_gpu_from_proc(gp, "/proc/driver/nvidia/gpus");

                LOG_DEBUG << gp.toString();
                m_gpu_pool.merge(gp);
                LOG_DEBUG << m_gpu_pool.toString();
            }
        }

        int32_t user_task_scheduling::load_task()
        {

            auto rtn = m_task_db.load_user_task(m_training_tasks);
            if (rtn != E_SUCCESS)
            {
                return rtn;
            }


            for (auto item: m_training_tasks)
            {
                auto task = item.second;
                //
                // support tasks run concurrently
                //
                if (task_queueing == task->status || task_pulling_image == task->status)
                {
                    m_queueing_tasks.push_back(task);
                    LOG_INFO << "user task scheduling insert ai training task to task queue, task id: "
                              << task->task_id << " container_id: " << task->container_id << " task status: "
                              << to_training_task_status_string(task->status);
                }
                else if (task_running == task->status ||update_task_error ==task->status)
                {
                    if (!m_gpu_pool.allocate(task->gpus))
                    {
                        LOG_ERROR << "out of gpu resource, " << "task id: " << task->task_id
                                  << ", gpu requirement "
                                  << task->gpus << ", gpu state " << m_gpu_pool.toString();
                        stop_task(task, task_out_of_gpu_resource);
                    }
                    else
                    {
                        LOG_DEBUG << m_gpu_pool.toString();
                    }

                    m_running_tasks[task->task_id] = task;
                    LOG_INFO << "user task scheduling insert ai training task to running task set, task id: "
                             << task->task_id << " container_id: " << task->container_id << " task status: "
                             << to_training_task_status_string(task->status);
                }
            }


            //sort by received_time_stamp
            m_queueing_tasks.sort(task_time_stamp_comparator());

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
            LOG_INFO << "exec_task start" ;
            int32_t ret ;

             if(task->server_specification=="restart")
            {

                ret = restart_task(task);
                m_queueing_tasks.remove(task);
                LOG_INFO << "move restart task from waiting queue map" << task->task_id;

                LOG_INFO << "restart_task over,ret:"<< ret;
                if (ret != E_SUCCESS)
                {
                     task->__set_status(task_stopped);
                     m_task_db.write_task_to_db(task);
                     LOG_ERROR << "restart user task failed, task id:" << task->task_id;
                     return E_DEFAULT;

                }

                else
                {
                    LOG_INFO << "task->status" << task->status;
                    //jimmy: move task from waiting queue  into running tasks map
                        LOG_INFO << "move task from waiting queue  into running tasks map" << task->task_id;
                        m_running_tasks[task->task_id] = task;
                        m_gpu_pool.allocate(task->gpus);
                        m_task_db.write_task_to_db(task);
                    LOG_INFO << "restart user task success, task id:" << task->task_id;
                        return E_SUCCESS;

                }

            }
            std::string operation =  m_container_worker->get_operation(task);
            LOG_INFO<< "task operation:" << operation;
            if(operation=="update")
            {    //update container
                LOG_INFO<< "task will update,  now erase task id:" << task->task_id;
                auto old_task =m_running_tasks[task->task_id];


                if (nullptr == old_task)
                {
                   // task->__set_status(task_stopped);

                  //  m_task_db.write_task_to_db(task);
                    if (!m_gpu_pool.check(get_gpu_spec(task->server_specification)))
                    {
                        LOG_ERROR << "update out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                                  << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                        task->__set_status(update_task_error);

                        m_task_db.write_task_to_db(task);
                        return E_DEFAULT;
                    }

                }else{

                    std::string old_gpus=old_task->gpus;
                    m_gpu_pool.free(old_gpus);// free old gpus
                    LOG_INFO<< "task will update,  old_gpus:" << old_gpus;
                    LOG_INFO<< "task will update, free m_gpu_pool:" << m_gpu_pool.toString();
                    if (!m_gpu_pool.check(get_gpu_spec(task->server_specification)))
                    {
                        LOG_ERROR << "update out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                                  << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                        task->__set_status(update_task_error);
                        m_gpu_pool.allocate(old_gpus);//add old gpus again
                        m_task_db.write_task_to_db(task);
                        return E_DEFAULT;
                    }
                    m_gpu_pool.allocate(old_gpus);//add old gpus again

                    LOG_INFO<< "task will update, allocate m_gpu_pool:" << m_gpu_pool.toString();
                    m_running_tasks.erase(task->task_id);
                }

                ret = update_task_commit_image(task);
                LOG_INFO<< "task will update,  now add task id again:" << task->task_id;
                m_running_tasks[task->task_id] = task;
                m_queueing_tasks.remove(task);
                LOG_INFO << "move task from waiting queue map" << task->task_id;
                if (ret == E_SUCCESS)
                {

                    LOG_INFO << "task->status" << task->status;
                    if (nullptr != old_task){

                        std::string old_gpus=old_task->gpus;
                        m_gpu_pool.free(old_gpus);// free old gpus
                    }

                    LOG_INFO<< "task will update, free m_gpu_pool:" << m_gpu_pool.toString();
                    m_gpu_pool.allocate(task->gpus);//add new gpus
                    LOG_INFO << "gpu state " << m_gpu_pool.toString();
                    m_task_db.write_task_to_db(task);
                    return E_SUCCESS;


                } else
                {

                    m_task_db.write_task_to_db(task);
                    return E_DEFAULT;

                }




            }else{

                // feng: validate task's gpu requirement
                if (task_queueing == task->status)
                {
                    if (!m_gpu_pool.check(task->gpus))
                    {
                        LOG_ERROR << "out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                                  << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                        stop_task(task, task_out_of_gpu_resource);
                        return E_DEFAULT;
                    }
                }

                ret = start_task(task);
                m_queueing_tasks.remove(task);
                LOG_INFO << "start_task over,ret:"<< ret;
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
                            m_task_db.write_task_to_db(task);
                            break;
                    }

                    LOG_ERROR << "start user task failed, task id:" << task->task_id;
                    return E_DEFAULT;
                }
                else
                {
                    LOG_INFO << "task->status" << task->status;
                    if (task->status == task_running)
                    {
                        //jimmy: move task from waiting queue  into running tasks map
                        LOG_INFO << "move task from waiting queue  into running tasks map" << task->task_id;
                        m_running_tasks[task->task_id] = task;

                       // m_gpu_pool.allocate(task->gpus);
                        if (!m_gpu_pool.allocate(task->gpus))
                        {
                            // is supposed never happen because gpu check passed before
                            LOG_INFO << "out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                                     << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                            stop_task(task, task_out_of_gpu_resource);
                            return E_DEFAULT;
                        }
                        else
                        {
                            LOG_INFO << "gpu state " << m_gpu_pool.toString();
                        }

                        return E_SUCCESS;

                    }
                    m_task_db.write_task_to_db(task);
                    return E_DEFAULT;

                }

                LOG_INFO << "start user task success, task id:" << task->task_id;

            }

            return E_SUCCESS;


        }



        int32_t user_task_scheduling::stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status)
        {

            // case 1: stop a task from running set
            if (task->status == task_running || task->status == update_task_error ||task->status > task_stopped ||task->status == task_queueing)
            {

                int32_t ret = task_scheduling::stop_task(task);
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "stop container error, container id: " << task->container_id << " task is:" << task->task_id;
                    return ret;
                }


            //    task->__set_end_time(time_util::get_time_stamp_ms());
                task->__set_status(end_status);
            //    m_task_db.write_task_to_db(task);

                m_running_tasks.erase(task->task_id);

                LOG_INFO << "stop container success, container id: " << task->container_id << " task is:" << task->task_id << " end time: "<< task->end_time;


                if ( task_out_of_gpu_resource != end_status )
                {
                    //free gpu resource
                    m_gpu_pool.free(task->gpus);
                }

                return E_SUCCESS;
            }


            // case 2: stop a task from waiting queue
            if (m_queueing_tasks.empty())
            {
                return E_SUCCESS;
            }
            auto top_task = m_queueing_tasks.front();
            m_queueing_tasks.remove(task);
           // if (task->status >= task_stopped)
          //  {
          //      return E_SUCCESS;
          //  }

//            if (task_running == task->status)
//            {
//                int32_t ret = task_scheduling::stop_task(task);
//                if (E_SUCCESS != ret)
//                {
//                    LOG_ERROR << "stop container error, container id: " << task->container_id << " task is:" << task->task_id;
//                }
//                LOG_INFO << "stop container success, container id: " << task->container_id << " task is:" << task->task_id;
//            }

            if (task_pulling_image == task->status)
            {
                stop_pull_image(task);
            }
            task->__set_end_time(time_util::get_time_stamp_ms());
            task->__set_status(end_status);
            m_task_db.write_task_to_db(task);
            
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


        int32_t  user_task_scheduling::add_update_task(std::shared_ptr<ai_training_task> task)
        {

            //flush to db
         //   if (E_SUCCESS != m_task_db.write_task_to_db(task))
         //   {
        //        return E_DEFAULT;
        //    }
         //   LOG_INFO << "update user task scheduling flush task to db: " << task->task_id;
          //  LOG_INFO << "update user task scheduling flush task to db memory: " << task->memory;
            m_queueing_tasks.push_back(task);
            LOG_INFO << "user task scheduling add m_training_tasks:" << task->task_id;
            m_training_tasks[task->task_id] = task;
            return E_SUCCESS;
        }


        int32_t user_task_scheduling::add_task(std::shared_ptr<ai_training_task> task)
        {
            if (m_training_tasks.size() > MAX_TASK_COUNT)
            {
                LOG_ERROR << "task is full.";
                return E_DEFAULT;
            }

            //flush to db
            if (E_SUCCESS != m_task_db.write_task_to_db(task))
            {
                return E_DEFAULT;
            }
            LOG_INFO << "user task scheduling flush task to db: " << task->task_id;
           // LOG_INFO << "user task scheduling flush task to db memory: " << task->memory;
            m_queueing_tasks.push_back(task);

            LOG_INFO << "user task scheduling add m_training_tasks:" << task->task_id;
            m_training_tasks[task->task_id] = task;

            return E_SUCCESS;
        }

        std::shared_ptr<ai_training_task> user_task_scheduling::find_task(std::string task_id)
        { 
            auto it_task = m_training_tasks.find(task_id);
            if (it_task != m_training_tasks.end())
            {
                return it_task->second;
            }

            //this may be never happend
//            std::string task_value;
//            leveldb::Status status = m_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
//            if (!status.ok() || status.IsNotFound())
//            {
//                return nullptr;
//            }
//
//            std::shared_ptr<ai_training_task> task_ptr = std::make_shared<ai_training_task>();
//            //deserialization
//            std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
//            buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
//            binary_protocol proto(buf.get());
//            task_ptr->read(&proto);

//            return task_ptr;

            return nullptr;
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

        int32_t user_task_scheduling::auth_task(std::shared_ptr<ai_training_task> task, bool is_normal_user_task)
        {
            auto ret = m_auth_task_handler != nullptr ? m_auth_task_handler(task) : E_SUCCESS;

            // let normal user task go if auth disable or network to auth center error.
            if(ret == E_NETWORK_FAILURE && is_normal_user_task)
            {
                ret = E_SUCCESS;
            }

            if(ret == E_BILL_DISABLE && is_normal_user_task)
            {
                ret = E_SUCCESS;
            }

            if (E_SUCCESS != ret && task->status < task_stopped)
            {
                LOG_ERROR << "auth failed. " << "drop task:" << task->task_id;
                stop_task(task, task_overdue_closed);
            }
            return ret;
        }

        int32_t user_task_scheduling::check_training_task_status(std::shared_ptr<ai_training_task> task)
        {
//            auto task = m_queueing_tasks.front();
            if (task == nullptr)
            {
                return E_NULL_POINTER;
            }



            //inspect container
            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->container_id);
            if (nullptr == resp)
            {
                LOG_ERROR << "user task check container error, container id: " << task->container_id;

                task->error_times++;
                //flush to db
                m_task_db.write_task_to_db(task);
                return E_DEFAULT;
            }else{
                task->error_times=0;
                m_task_db.write_task_to_db(task);
            }

            //judge retry times
            if (task->error_times > AI_TRAINING_MAX_RETRY_TIMES)
            {
                LOG_WARNING << "user task restart container too many times and close task, " << "task id: " << task->task_id << " container id: " << task->container_id;
                stop_task(task, task_abnormally_closed);
                return E_DEFAULT;
            }

            if (true == resp->state.running)
            {
                //LOG_INFO << "ai power provider service check container is running, " << "task id: " << task->task_id << " container id: " << task->container_id;
                auth_task(task);
                return E_SUCCESS;
            }

            else if (0 != resp->state.exit_code && 137 != resp->state.exit_code)
            {
                LOG_INFO << "inspect container not running, " << "task id: " << task->task_id << " container id: " << task->container_id << " exit_code" << resp->state.exit_code;
               // stop_task(task, task_abnormally_closed);
                start_task(task);
                return E_SUCCESS;
            }
            else
            {
                LOG_INFO << "user task inspect container success closed, " << "task id: " << task->task_id << " container id: " << task->container_id  << " exit_code" << resp->state.exit_code;
                stop_task(task, task_successfully_closed);
              //  start_task(task);
                return E_SUCCESS;
            }
            return E_SUCCESS;
        }

        int32_t user_task_scheduling::process_task()
        {

            // jimmy: support task runs in concurrently

            // step 1: process tasks in waiting queue
            if (!m_queueing_tasks.empty())
            {

                auto task = m_queueing_tasks.front();
                if (task_queueing == task->status)
                {
                    return exec_task();
                }
                else if (task_pulling_image == task->status)
                {
                    return check_pull_image_state();
                }
//            else if (task_running == task->status)
//            {
//                LOG_DEBUG << "training start check ai training task: " << task->task_id << " status: " << to_training_task_status_string(task->status);;
//                return check_training_task_status();
//            }
                else
                {
                    //drop this task.
                    m_queueing_tasks.pop_front();
                    LOG_ERROR << "training start exec ai training task: " << task->task_id << " invalid status: "
                              << to_training_task_status_string(task->status);
                    return E_DEFAULT;
                }
            }

            // step 2: process tasks in running set

            for (auto& each : m_running_tasks)
            {
                check_training_task_status(each.second);
            }

            return E_SUCCESS;
        }

        int32_t user_task_scheduling::prune_task()
        {
            prune_task(m_prune_intervel);

            return E_SUCCESS;
        }

        /**
         *  This method will check all stopped user tasks and delete them from db and cache.
         *  Docker container will be removed by common_service::on_timer_prune_container.
         * @param interval, threshold that system will wait before deleting a stoppped task.
         * @return
         */
        int32_t user_task_scheduling::prune_task(int16_t interval)
        {
            int64_t cur = time_util::get_time_stamp_ms();

            int64_t p_interval = (int64_t) interval * 3600 * 1000;

            LOG_INFO << "prune docker container." << " interval:" << interval << "h";

            for (auto task_iter = m_training_tasks.begin(); task_iter != m_training_tasks.end();)
            {
                if (task_iter->second->status < task_stopped)
                {
                    task_iter++;
                    continue;
                }

                // container_id is empty when the task fail to exectue.
                // note: client node may fail to update its task status after task pruned from the computing node.
                bool enable_delete = false;
                auto task = task_iter->second;
                if ( (cur - task->end_time) > p_interval)
                {
                    enable_delete = true;
                    LOG_INFO << "prune long duration stopped task " << task->task_id;
                }
                else if (task->container_id.empty())
                {
                    enable_delete = true;
                    LOG_INFO << "prune fail to exeucte: " << task->task_id;
                }

                if (enable_delete)
                {
                    m_task_db.delete_task(task);
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

        int32_t user_task_scheduling::process_urgent_task(std::shared_ptr<ai_training_task> task)
        {

            if (!task)
                return E_DEFAULT;

            if (task->code_dir == NODE_REBOOT)
            {
                if (E_SUCCESS != auth_task(task, false))
                {
                    LOG_INFO << "auth fail for node reboot";
                    return E_DEFAULT;
                }

                // reboot node
                LOG_WARNING << "node reboot now";
                system("wall \"System will reboot by dbc in one minute!\"");
                system("sudo /sbin/shutdown -r +1");
                return E_SUCCESS;

            }

            return E_SUCCESS;
        }

        std::string user_task_scheduling::get_gpu_state()
        {
            return m_gpu_pool.toString();
        }

        std::string user_task_scheduling::get_active_tasks()
        {
            std::string s;
            for (auto & kvp : m_running_tasks)
            {
                if(s.empty())
                {
                    s="[";
                }
                else
                {
                    s+=",";
                }

                s+="\""+kvp.first+"\"";
            }

            if(s.empty())
            {
                s = "[]";
            }
            else
            {
                s += "]";
            }
            return s;
        }

    }
}