/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   cmd_request_service.h
* description    :   cmd_request_service
* date                  :   2018.01.28
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include "db/ai_db_types.h"
#include <string>
#include "service_module.h"
#include "db/ai_requestor_task_db.h"

using namespace matrix::core;
namespace fs = boost::filesystem;

#define LIST_TRAINING_TIMER         "list_training_timer"
#define TASK_LOGS_TIMER             "task_logs_timer"

namespace ai {
    namespace dbc {
        class cmd_request_service : public service_module {
        public:
            cmd_request_service() = default;

            ~cmd_request_service() override = default;

            std::string module_name() const override {
                return "cmd_request_service";
            }

        protected:
            void init_timer() override;

            void init_invoker() override;

            void init_subscription() override;

            int32_t service_init(bpo::variables_map &options) override;

            int32_t on_cmd_create_task_req(std::shared_ptr<message> &msg);

            int32_t on_cmd_start_task_req(std::shared_ptr<message> &msg);

            int32_t on_cmd_restart_task_req(std::shared_ptr<message> &msg);

            std::shared_ptr<message> create_node_create_task_req_msg(bpo::variables_map &vm, ai::dbc::cmd_task_info &task_info);

            std::shared_ptr<message> create_node_start_task_req_msg(bpo::variables_map &vm, ai::dbc::cmd_task_info &task_info);

            std::shared_ptr<message> create_node_restart_task_req_msg(bpo::variables_map &vm, ai::dbc::cmd_task_info &task_info);

            int32_t on_cmd_stop_task_req(const std::shared_ptr<message> &msg);

            int32_t on_cmd_clean_task_req(const std::shared_ptr<message> &msg);

            int32_t on_cmd_task_logs_req(const std::shared_ptr<message> &msg);

            int32_t on_cmd_list_task_req(const std::shared_ptr<message> &msg);

            int32_t validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string &error);

            int32_t validate_ipfs_path(const std::string &path_arg);

            int32_t validate_entry_file_name(const std::string &entry_file_name);

            int32_t on_list_training_resp(std::shared_ptr<message> &msg);

            int32_t on_logs_resp(std::shared_ptr<message> &msg);

            int32_t on_binary_forward(std::shared_ptr<message> &msg);

            int32_t on_list_training_timer(std::shared_ptr<core_timer> timer);

            int32_t on_logs_timer(std::shared_ptr<core_timer> timer);

            bool precheck_msg(std::shared_ptr<message> &msg);

        protected:
            ai::dbc::ai_requestor_task_db m_req_training_task_db;
        };
    }
}
