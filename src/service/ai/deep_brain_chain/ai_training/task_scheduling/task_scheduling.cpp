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

#include <regex>
#include "time_util.h"
#include "url_validator.h"
#include <boost/property_tree/json_parser.hpp>
#include <unistd.h>
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

        int32_t task_scheduling::update_task_commit_image(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }

            if (task->task_id.empty())
            {
                LOG_DEBUG << "task config error.";
                task->__set_status(update_task_error);
                task->error_times = 0;

                return E_DEFAULT;
            }


            std::string autodbcimage_version=m_container_worker->get_autodbcimage_version(task);
            LOG_INFO << "autodbcimage_version" << autodbcimage_version;
            if(autodbcimage_version.empty())
            {
                task->__set_status(update_task_error);
                task->error_times = 0;

                return E_DEFAULT;
            }

            std::string training_engine_name="www.dbctalk.ai:5000/dbc-free-container:autodbcimage_"+task->task_id.substr(0,6)+"_"+task->container_id.substr(0,6)+autodbcimage_version;
            std::string image_id="";
            bool can_create_container=false;
            LOG_INFO << "task->status:" << task->status ;
            LOG_INFO << "training_engine_name 1 "<<training_engine_name;
            if (task->status!=task_creating_image ){ //刚开始创建

                std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->container_id);//需要查询一下task中的镜像名字是否和真正的容器id一致

                if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
                {
                    std::string container_id=CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                    if(container_id.empty())
                    {
                        container_id=CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                    }

                    if(!container_id.empty())
                    {
                        resp = CONTAINER_WORKER_IF->inspect_container(container_id);
                    }

                    if (resp == nullptr)
                    {
                        return E_DEFAULT;
                    }

                    task->__set_container_id(container_id);
                }
                training_engine_name="www.dbctalk.ai:5000/dbc-free-container:autodbcimage_"+task->task_id.substr(0,6)+"_"+task->container_id.substr(0,6)+autodbcimage_version;
                LOG_INFO << "training_engine_name 2 "<<training_engine_name;

                if(E_SUCCESS==CONTAINER_WORKER_IF-> exist_docker_image(training_engine_name,30)) {

                    LOG_INFO << "delete the same name image";
                    CONTAINER_WORKER_IF->delete_image(training_engine_name);//删除之前的同名镜像
                    sleep(10);
                }
                int64_t sleep_time=m_container_worker->get_sleep_time(task);
                task->__set_start_time(time_util::get_time_stamp_ms());
                LOG_INFO << "sleep_time waiting :" << sleep_time << "s" ;
                task->__set_status(task_creating_image);
                std::string status=CONTAINER_WORKER_IF->commit_image(task->container_id,autodbcimage_version,task->task_id,60);
                if(status.compare("error")==0){
                    return E_DEFAULT;
                }
                return E_SUCCESS;

            } else if (task->status==task_creating_image){ //正在创建中

                if(E_SUCCESS==CONTAINER_WORKER_IF-> exist_docker_image(training_engine_name,60)) {
                    LOG_INFO << "is  exist_docker_image";
                    //  sleep(100);

                    can_create_container=true;


                }else
                {
                    int64_t real_time=time_util::get_time_stamp_ms();
                    int64_t sleep_time=m_container_worker->get_sleep_time(task);
                    int64_t sub_time=real_time - task->start_time;

                    LOG_INFO << "real_time  ：" << real_time;
                    LOG_INFO << "task  start_time：" << task->start_time;
                    LOG_INFO << "sub_time：" << sub_time;

                    if(sub_time>sleep_time*1000){//是否创建时间已经超过sleep_time
                   //     if(sub_time>sleep_time*10){//测试
                        can_create_container= false;

                    } else
                    {
                        return E_SUCCESS;
                    }

                }


            }


             if(can_create_container)
             {
                std::string training_engine_original=task->training_engine;
             //   training_engine_new="www.dbctalk.ai:5000/dbc-free-container:autodbcimage_"+task->task_id.substr(0,6)+"_"+task->container_id.substr(0,6)+autodbcimage_version;
                task->__set_training_engine(training_engine_name);
                LOG_INFO << "training_engine_original:" << training_engine_original;
                LOG_INFO << "training_engine_new:" << "www.dbctalk.ai:5000/dbc-free-container:autodbcimage_"+task->task_id.substr(0,6)+"_"+task->container_id.substr(0,6)+autodbcimage_version;
                int32_t status=start_task_from_new_image(task,autodbcimage_version,training_engine_name);

                if(status!= E_NO_START_CONTAINER &&status!= E_SUCCESS)
                {
                    task->__set_training_engine(training_engine_original);
                    return E_DEFAULT;
                }


            }else
            {

              //  CONTAINER_WORKER_IF->delete_image(image_id);//delete new image,防止可能创建成功
                LOG_INFO << "update_task_error";
                task->__set_status(update_task_error);
                task->error_times = 0;

                return E_DEFAULT;
            }

            task->__set_gpus(get_gpu_spec(task->server_specification));
            return E_SUCCESS;
        }

        std::string task_scheduling::get_gpu_spec(std::string s)
        {
            if (s.empty())
            {
                return "";
            }

            std::string rt;
            std::stringstream ss;
            ss << s;
            boost::property_tree::ptree pt;

            try
            {
                boost::property_tree::read_json(ss, pt);
                rt = pt.get<std::string>("env.NVIDIA_VISIBLE_DEVICES");

                if ( !rt.empty())
                {
                    matrix::core::string_util::trim(rt);
                    LOG_DEBUG << "gpus requirement: " << rt;
                }
            }
            catch (...)
            {

            }

            return rt;
        }

        int32_t task_scheduling::change_gpu_id(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_DEFAULT;
            }

            if (task->task_id.empty())
            {
                LOG_DEBUG << "task config error.";
                task->__set_status(update_task_error);
                return E_DEFAULT;
            }

            if (task->status!=task_creating_image ) { //刚开始创建

                std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
                        task->container_id);//需要查询一下task中的镜像名字是否和真正的容器id一致

                if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
                {
                    std::string container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                    if (container_id.empty()) {
                        container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                    }

                    if (!container_id.empty()) {
                        resp = CONTAINER_WORKER_IF->inspect_container(container_id);
                    }

                    if (resp == nullptr) {
                        return E_DEFAULT;
                    }

                    task->__set_container_id(container_id);
                }


                std::string change_gpu_id_file_name = env_manager::get_home_path().generic_string() + "/tool/change_gpu_id.sh";
                std::string task_id=task->task_id;

                std::string old_gpu_id = m_container_worker->get_old_gpu_id(task);
                std::string new_gpu_id = m_container_worker->get_new_gpu_id(task);

                std::string old_cpu_shares=std::to_string(m_container_worker->get_old_cpu_shares(task));
                std::string new_cpu_shares=std::to_string(m_container_worker->get_new_cpu_shares(task));
                std::string cpu_shares=old_cpu_shares+","+new_cpu_shares;

                std::string old_cpu_quota=std::to_string(m_container_worker->get_old_cpu_quota( task));
                std::string new_cpu_quota=std::to_string(m_container_worker->get_new_cpu_quota(task));
                std::string cpu_quota= old_cpu_quota+","+new_cpu_quota;

                std::string old_memory = m_container_worker->get_old_memory(task);
                std::string new_memory=m_container_worker->get_new_memory(task);
                std::string memory=old_memory +","+ new_memory;

                std::string old_memory_swap=m_container_worker->get_old_memory_swap(task);
                std::string new_memory_swap=m_container_worker->get_new_memory_swap(task);
                std::string memory_swap=old_memory_swap+","+new_memory_swap;


                std::string container_id=task->container_id;
                std::string m_change_gpu_id_cmd="";
                m_change_gpu_id_cmd = boost::str(boost::format("/bin/bash %s %s %s %s %s %s %s %s %s")
                        % change_gpu_id_file_name % task_id % old_gpu_id % new_gpu_id % container_id % cpu_shares  % cpu_quota
                        % memory  % memory_swap);

                LOG_INFO << "m_change_gpu_id_cmd " << m_change_gpu_id_cmd;
                LOG_INFO << " m_change_gpu_id_cmd  will commit" ;
                if (m_change_gpu_id_cmd.empty())
                {
                    LOG_ERROR << "m_change_gpu_id_cmd command is empty";
                    return E_DEFAULT;
                }
                try
                {
                    std::error_code ec;
                    std::future<std::string> fut;
                    std::future<std::string> fut_err;
                    int32_t ret = bp::system(bp::cmd=m_change_gpu_id_cmd, bp::std_out > fut, bp::std_err > fut_err, ec);
                    std::string m_change_gpu_id_cmd_log = "m_change_gpu_id_cmd info.";
                    if (ec)
                    {
                        m_change_gpu_id_cmd_log +=ec.message();
                    }

                    if (fut.valid())
                    {
                        m_change_gpu_id_cmd_log += fut.get();
                    }
                    if (fut_err.valid())
                    {
                        m_change_gpu_id_cmd_log += fut_err.get();
                    }

                     LOG_INFO << " m_change_gpu_id_cmd ret code:" << ret << ". " ;


                }
                catch (const std::exception & e)
                {
                    LOG_ERROR << "m_change_gpu_id_cmd error" << e.what();
                    return E_DEFAULT;
                }
                catch (...)
                {
                    LOG_ERROR << "m_change_gpu_id_cmd error";
                    return E_DEFAULT;
                }
                LOG_INFO << "sleep_time waiting"  ;
                int64_t sleep_time=m_container_worker->get_sleep_time(task);
                task->__set_start_time(time_util::get_time_stamp_ms());
                LOG_INFO << "sleep_time waiting :" << sleep_time << "s" ;
                task->__set_status(task_creating_image);
                return E_SUCCESS;

            }else if (task->status==task_creating_image){ //正在创建中

                std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->container_id);
                if (resp!= nullptr && true == resp->state.running) {

                    std::string sub_task_id=task->task_id.substr(0,12);
                    std::string gpu_id=CONTAINER_WORKER_IF->get_gpu_id(sub_task_id);
                    if(gpu_id.find("NVIDIA_VISIBLE_DEVICES=")== string::npos){ //如果gpu id 没有修改过来
                        LOG_INFO << "update_task_error";
                        task->__set_status(update_task_error);

                        return E_DEFAULT;
                    }else{

                        std::vector<std::string> list;
                        string_util::split( gpu_id, "=", list);
                        std::string new_gpu_id = m_container_worker->get_new_gpu_id(task);
                        if(new_gpu_id.compare(list[1])!=0){//说明没有设置成功
                            LOG_INFO << "update_task_error new_gpu_id !=inspect " <<gpu_id;
                            task->__set_status(update_task_error);

                            return E_DEFAULT;
                        }
                    }

                }else
                {
                    int64_t real_time=time_util::get_time_stamp_ms();
                    int64_t sleep_time=m_container_worker->get_sleep_time(task);
                    int64_t sub_time=real_time - task->start_time;

                    LOG_INFO << "real_time  ：" << real_time;
                    LOG_INFO << "task  start_time：" << task->start_time;
                    LOG_INFO << "sub_time：" << sub_time;

                    if(sub_time>300*1000 ){//是否创建时间已经超过sleep_time

                        LOG_INFO << "update_task_error ：can not start container  ";
                        task->__set_status(update_task_error);
                        return E_DEFAULT;

                    }

                    if(sub_time>60*1000){//是否创建时间已经超过60s

                       // CONTAINER_WORKER_IF->start_container(task->container_id);//说明脚本没有启动容器成功，再次启动
                       // sleep(15);
                    }



                    return E_SUCCESS;

                }


            }

            task->__set_gpus(get_gpu_spec(task->server_specification));

            task->__set_status(task_running);

            return E_SUCCESS;

        }





       //from new image
        int32_t task_scheduling::start_task_from_new_image(std::shared_ptr<ai_training_task> task,std::string autodbcimage_version,std::string training_engine_new)
        {
            if (nullptr == task)
            {

                return E_DEFAULT;
            }



            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(task->container_id);
            std::string old_container_id=task->container_id;
            if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
            {
                std::string container_id=CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                if(container_id.empty())
                {
                    container_id=CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                }

                if(!container_id.empty())
                {
                    resp = CONTAINER_WORKER_IF->inspect_container(container_id);
                }

                if (resp == nullptr)
                {
                    return E_DEFAULT;
                }
                old_container_id=container_id;
                task->__set_container_id(container_id);
            }

            bool original_status_container=false;//默认容器是关闭的状态
            if (true == resp->state.running)
            {
                original_status_container=true;
                if(E_SUCCESS==CONTAINER_WORKER_IF->stop_container(old_container_id))
                {
                    LOG_INFO << "stop container success , task id:" << old_container_id;


                } else
                {
                    LOG_INFO << "stop container failure , task id:" << task->task_id;
                    task->__set_status(update_task_error);
                    task->error_times = 0;
                  //  m_task_db.write_task_to_db(task);
                    return E_DEFAULT;

                }
            }


            if (E_SUCCESS != create_task_from_image(task,autodbcimage_version))
            {
                LOG_ERROR << "create task error";
                CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
                if(original_status_container)
                {
                    CONTAINER_WORKER_IF->start_container(task->container_id);//start original container_id
                }

                task->__set_status(update_task_error);
                task->error_times = 0;
               // m_task_db.write_task_to_db(task);
                return E_DEFAULT;
            }else{

          /*      std::string image_id=CONTAINER_WORKER_IF->get_image_id(old_container_id);

                if(image_id.compare("error")==0){
                    CONTAINER_WORKER_IF->remove_container(task->container_id);//delete new docker
                    CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
                    CONTAINER_WORKER_IF->start_container(old_container_id);//start original container_id
                    task->__set_container_id(old_container_id);
                    task->__set_status(update_task_error);
                    task->error_times = 0;
                  //  m_task_db.write_task_to_db(task);
                    LOG_INFO << "remove container failure. recover old_container:" << task->task_id;
                    return E_DEFAULT;
                }*/

              //  bool can_delete_image=CONTAINER_WORKER_IF->can_delete_image(image_id);//is or not delete image,can not delete original images

                std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(old_container_id);
                bool is_exsit_old_container=false;
                if(resp == nullptr)
                {
                    sleep (3);
                    resp = CONTAINER_WORKER_IF->inspect_container(old_container_id);
                }

                if(resp == nullptr) //用第二种方法判断 旧容器知否存在
                {
                    sleep (6);
                    std::string container_id=CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
                    if(!container_id.empty())
                    {
                        is_exsit_old_container=true;
                    }

                }

                if (resp != nullptr || is_exsit_old_container)//如果旧的镜像还存在，则删除
                {
                    if(E_SUCCESS!=CONTAINER_WORKER_IF->remove_container(old_container_id))//delete old container
                    {
                        CONTAINER_WORKER_IF->remove_container(task->container_id);//delete new docker
                        CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
                        CONTAINER_WORKER_IF->start_container(old_container_id);//start original container_id
                        task->__set_container_id(old_container_id);
                        task->__set_status(update_task_error);
                        task->error_times = 0;
                        //  m_task_db.write_task_to_db(task);
                        LOG_INFO << "remove container failure. recover old_container:" << task->task_id;
                        return E_DEFAULT;
                    }//else// if(can_delete_image){//旧的镜像 因为有新生成的子镜像，故无法删除

                    //if(E_SUCCESS!=CONTAINER_WORKER_IF->remove_image(image_id)){//delete old image
                    //     CONTAINER_WORKER_IF->remove_image(image_id);
                    // }
                    // }
                }


                LOG_INFO << "delete old container success , task id:" << old_container_id;
                if(E_SUCCESS!=CONTAINER_WORKER_IF->rename_container(task->task_id,autodbcimage_version))
                {
                    LOG_INFO << "rename container failure";

                    CONTAINER_WORKER_IF->rename_container(task->task_id,autodbcimage_version);

                }

                LOG_INFO << "rename container success , task id:" << task->task_id;
            }
            LOG_INFO << "start_task_from_new_image success. Task id:" ;

            int32_t ret = CONTAINER_WORKER_IF->start_container(task->container_id);//start new container_id

            if (ret != E_SUCCESS)
            {
                task->__set_status(update_task_error);
                LOG_ERROR << "Start task error. Task id:" << task->task_id;
                return E_NO_START_CONTAINER;
            }

            LOG_INFO << "start_task_from_new_image success. Task id:" << task->task_id;
           // task->__set_start_time(time_util::get_time_stamp_ms());
            task->__set_status(task_running);
            task->error_times = 0;
            LOG_INFO << "update task status:" << "task_running";
          //  m_task_db.write_task_to_db(task);
            LOG_INFO << "update task status:" << "write_task_to_db";
            LOG_INFO << "update E_SUCCESS:" << E_SUCCESS;
            return E_SUCCESS;
        }

        vector<string> split(const string& str, const string& delim) {
            vector<string> res;
            if("" == str) return res;
            //先将要切割的字符串从string类型转换为char*类型
            char * strs = new char[str.length() + 1] ; //不要忘了
            strcpy(strs, str.c_str());

            char * d = new char[delim.length() + 1];
            strcpy(d, delim.c_str());

            char *p = strtok(strs, d);
            while(p) {
                string s = p; //分割得到的字符串转换为string类型
                res.push_back(s); //存入结果数组
                p = strtok(NULL, d);
            }

            return res;
        }


        int32_t task_scheduling::create_task_from_image(std::shared_ptr<ai_training_task> task,std::string  autodbcimage_version)
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

         //   std::string container_id_original=task->container_id;
            std::shared_ptr<container_config> config = m_container_worker->get_container_config_from_image(task);
            std::string task_id=task->task_id;



           std::string container_name=task_id+"_DBC_"+autodbcimage_version;
           if(CONTAINER_WORKER_IF->exist_container(container_name)!=E_CONTAINER_NOT_FOUND){
               CONTAINER_WORKER_IF->remove_container(container_name);
           }
            std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task_id,autodbcimage_version);

            if (resp != nullptr && !resp->container_id.empty())
            {
                task->__set_container_id(resp->container_id);
                LOG_INFO << "create from_image task success. task id:" << task->task_id << " container id:" << task->container_id;

                return E_SUCCESS;
            }
            else
            {
               // sleep(90);
                LOG_INFO << "exist_container ?" ;
                if(CONTAINER_WORKER_IF->exist_container_time(container_name,240)!=E_CONTAINER_NOT_FOUND) {
                    LOG_INFO << "exist_container yes";
                }else{
                    LOG_ERROR << "create task failed. task id:" << task->task_id;
                }

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

            if(CONTAINER_WORKER_IF->exist_container(task->task_id)!=E_CONTAINER_NOT_FOUND){
                CONTAINER_WORKER_IF->remove_container(task->task_id);
            }

            std::shared_ptr<container_config> config = m_container_worker->get_container_config(task);
            std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task->task_id,"");
            if (resp != nullptr && !resp->container_id.empty())
            {
                task->__set_container_id(resp->container_id);
                LOG_INFO << "create task success. task id:" << task->task_id << " container id:" << task->container_id;

                return E_SUCCESS;
            }
            else
            {
                sleep(60);
                std::string container_name=task->task_id;
                LOG_INFO << "exist_container ?" ;
                std::string container_id=CONTAINER_WORKER_IF->get_container_id(container_name);

                if(container_id.compare("error")!=0){
                    LOG_INFO << "exist_container yes" ;
                    task->__set_container_id(container_id);
                    LOG_INFO << "create task success. task id:" << task->task_id << " container id:" << task->container_id;

                    return E_SUCCESS;
                }else{
                    LOG_INFO << "exist_container no" ;
                    LOG_ERROR << "create task failed. task id:" << task->task_id;
                }

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

                int32_t ret = CONTAINER_WORKER_IF->restart_container(task->container_id);

                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "restart task error. Task id:" << task->task_id;
                    LOG_ERROR << "restart task error. container_id:" << task->container_id;
                    return E_DEFAULT;
                }
                LOG_DEBUG << "task have been restarted, task id:" << task->task_id;
                task->__set_start_time(time_util::get_time_stamp_ms());
                task->__set_status(task_running);
                task->error_times = 0;
                LOG_INFO << "task status:" << "task_running";
                m_task_db.write_task_to_db(task);
                LOG_INFO << "task status:" << "write_task_to_db";
                LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
                return E_SUCCESS;
            }

            //if image do not exist, then pull it
            if (E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(task->training_engine,15))
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

            //  if (is_container_existed)
            //  {
            // server_specification indicates the container to be reused for this task
            // needs to indicate container run with different parameters
            // text += ("code_dir=" + task->code_dir + "\n");

            //  if (task->server_specification == "restart")
            // {
            // use case: restart a task
            //      text += ("restart=true\n");
            //  }
            // }


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

        int32_t task_scheduling::restart_task(std::shared_ptr<ai_training_task> task)
        {
            if (nullptr == task)
            {
                return E_SUCCESS;
            }if (task->status == update_task_error)//如果任务是升级错误状态，则只修改任务状态为running
            {
                LOG_INFO << "update task status success. Task id:" << task->task_id;
                task->__set_start_time(time_util::get_time_stamp_ms());
                task->__set_status(task_running);
                task->error_times = 0;
                LOG_INFO << "task status:" << "task_running";
                m_task_db.write_task_to_db(task);
                LOG_INFO << "task status:" << "write_task_to_db";
                LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
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

                int32_t ret = CONTAINER_WORKER_IF->restart_container(task->container_id);

                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "restart task error. Task id:" << task->task_id;
                    LOG_ERROR << "restart task error. container_id:" << task->container_id;
                    return E_DEFAULT;
                }
                LOG_DEBUG << "task have been restarted, task id:" << task->task_id;

                return E_SUCCESS;
            } else if(DBC_TASK_STOPPED == state)
            {
                int32_t ret = CONTAINER_WORKER_IF->start_container(task->container_id);

                if (ret != E_SUCCESS)
                {
                    LOG_ERROR << "Start task error. Task id:" << task->task_id;
                    return E_DEFAULT;
                }
            } else
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
            if(task->container_id.empty()){
                return CONTAINER_WORKER_IF->stop_container(task->task_id);
            }else{
                return CONTAINER_WORKER_IF->stop_container(task->container_id);
            }

        }

        int32_t task_scheduling::stop_task_only_id(std::string task_id)
        {

            return CONTAINER_WORKER_IF->stop_container(task_id);


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
            std::string container_id = task->container_id;

            // container can be started again by task delivered latter,
            // in that case, the container's id and name keeps the original value, then new task's id and container's name does not equal any more.
            if(!task->container_id.empty())
            {
                container_id = task->container_id;
            }

            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(container_id);
            if (nullptr == resp)
            {
               // LOG_ERROR << "set_container_id:" << "null";
                //task->__set_container_id("");
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