/*********************************************************************************
*  Copyright (c) 2017-2021 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   vm_client.h
* description    :   vm client for definition
* date                  :   2021.02.08
* author            :   Feng
**********************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "http_client.h"
#include "vm_message.h"


using namespace std;


#define DEFAULT_STOP_VM_TIME                         2                              //stop time out


namespace matrix
{
    namespace core
    { 

        class vm_client
        {
        public:

            vm_client(std::string remote_ip, uint16_t remote_port);

            ~vm_client() = default;

            //create
            std::shared_ptr<vm_create_resp> create_vm(std::shared_ptr<vm_config> config);

            std::shared_ptr<vm_create_resp> create_vm(std::shared_ptr<vm_config> config, std::string name,std::string autodbcimage_version);
            int32_t rename_vm(std::string name,std::string autodbcimage_version);
            int32_t restart_vm(std::string vm_id);
            int32_t update_vm(std::string vm_id, std::shared_ptr<update_vm_config> config);
            std::string get_commit_image(std::string vm_id,std::string version,std::string task_id,int32_t sleep_time);
            //start
            int32_t start_vm(std::string vm_id);

            int32_t start_vm(std::string vm_id, std::shared_ptr<vm_host_config> config);

            //stop
            int32_t stop_vm(std::string vm_id);

            int32_t stop_vm(std::string vm_id, int32_t timeout);

            //wait
            int32_t wait_vm(std::string vm_id);

            //remove
            int32_t remove_vm(std::string vm_id);

            int32_t remove_vm(std::string vm_id, bool remove_volumes);

            //prune vm
            int32_t prune_vm(int16_t interval);

            int32_t prune_images();
            int32_t delete_image(std::string id);
            std::shared_ptr<images_info> get_images();
            //inspect
            std::shared_ptr<vm_inspect_response> inspect_vm(std::string vm_id);
            bool  can_delete_image(std::string image_id);
            //logs
            std::shared_ptr<vm_logs_resp> get_vm_log(std::shared_ptr<vm_logs_req> req);
            std::string get_image_id(std::string vm_id);
            int32_t remove_image(std::string image_id);
            void set_address(std::string remote_ip, uint16_t remote_port);

            int32_t exist_docker_image(const std::string & image_name,int32_t sleep_time);

            std::shared_ptr<docker_info> get_docker_info();
            std::string get_vm(const std::string user_vm_name);
            std::string get_running_vm();
            void del_images();
            int32_t exist_vm(const std::string & vm_name);
            std::string get_vm_id(std::string vm_name);
            std::string commit_image(std::string vm_id,std::string version,std::string task_id,int32_t sleep_time);
            int32_t exist_vm_time(const std::string & vm_name,int32_t sleep_time);
            std::string get_vm_size(std::string id);
            std::string get_running_vm_no_size();
            std::string get_vm_id_current(std::string task_id);
            std::string get_vm_size_by_task_id(std::string id);
            std::string get_images_original();
            std::string get_gpu_id(std::string vm_id);
            std::string get_docker_dir(std::string vm_id);
        protected:

            http_client m_http_client;

            std::string m_remote_ip;

            uint16_t m_remote_port;

            std::shared_ptr<docker_info> m_docker_info_ptr;

          //  std::shared_ptr<images_info> m_list_images_info_ptr;
        };

    }

}
