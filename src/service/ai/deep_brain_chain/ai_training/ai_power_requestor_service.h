/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ai_power_requestor_service.h
* description    :   ai_power_requestor_service
* date                  :   2018.01.28
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include <leveldb/db.h>
#include "ai_db_types.h"
#include <string>
#include "service_module.h"


using namespace matrix::core;

namespace fs = boost::filesystem;


#define DEFAULT_SERVICE_TIMER_INTERVAL                     (8 * 1000)                                         //8s
#define LIST_TRAINING_TIMER                                         "list_training_timer"
#define TASK_LOGS_TIMER                                                  "task_logs_timer"


namespace ai
{
    namespace dbc
    {

        class ai_power_requestor_service : public service_module
        {
        public:

            ai_power_requestor_service();

            virtual ~ai_power_requestor_service() = default;

            virtual std::string module_name() const { return ai_power_requestor_service_name; }

        protected:

            int32_t init_conf();

            void init_subscription();

            void init_invoker();

            int32_t init_db();

            void init_timer();

            virtual int32_t service_init(bpo::variables_map &options);

        protected:

            int32_t on_cmd_start_training_req(std::shared_ptr<message> &msg);
            int32_t on_cmd_start_multi_training_req(std::shared_ptr<message> &msg);

            int32_t on_cmd_stop_training_req(const std::shared_ptr<message> &msg);

            int32_t on_cmd_list_training_req(const std::shared_ptr<message> &msg);
            int32_t on_list_training_resp(std::shared_ptr<message> &msg);

            int32_t on_cmd_logs_req(const std::shared_ptr<message> &msg);
            int32_t on_logs_resp(std::shared_ptr<message> &msg);

            int32_t validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string& error);
            int32_t validate_ipfs_path(const std::string &path_arg);
            int32_t validate_entry_file_name(const std::string &entry_file_name);

            int32_t on_list_training_timer(std::shared_ptr<core_timer> timer);
            int32_t on_logs_timer(std::shared_ptr<core_timer> timer);
            int32_t on_cmd_task_clean(const std::shared_ptr<message> &msg);

        protected:

            std::shared_ptr<message> create_task_msg(const std::string &task_file, const bpo::options_description &opts, ai::dbc::cmd_task_info & task_info);

            std::shared_ptr<message> create_task_msg(bpo::variables_map& vm, ai::dbc::cmd_task_info & task_info);

            bool write_task_info_to_db(ai::dbc::cmd_task_info &task_info);

            bool read_task_info_from_db(std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status = 0);

            bool read_task_info_from_db(std::list<std::string> task_ids, std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status = 0);

            bool read_task_info_from_db(std::string task_id, ai::dbc::cmd_task_info &task_info);

            bool is_task_exist_in_db(std::string task_id);

            //int32_t on_cmd_clear(const std::shared_ptr<message> &msg);
            int32_t on_cmd_ps(const std::shared_ptr<message> &msg);


        protected:

            std::shared_ptr<leveldb::DB> m_req_training_task_db;
        };

    }

}

