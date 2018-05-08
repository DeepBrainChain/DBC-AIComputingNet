/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºai_power_requestor_service.h
* description    £ºai_power_requestor_service
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include <boost/asio.hpp>
#include <leveldb/db.h>
#include "db_types.h"
#include <string>
#include "task_common_def.h"
#include "service_module.h"


using namespace matrix::core;
using namespace boost::asio::ip;

namespace fs = boost::filesystem;


#define LIST_TRAINING_TIMER_INTERVAL                     (25 * 1000)                                         //25s
#define LIST_TRAINING_TIMER                                         "list_training_timer"


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

            int32_t validate_cmd_training_task_conf(const bpo::variables_map &vm);

            int32_t on_list_training_timer(std::shared_ptr<core_timer> timer);

        protected:

            void add_task_config_opts(bpo::options_description &opts) const;

            std::shared_ptr<message> create_task_msg_from_file(const std::string &task_file, const bpo::options_description &opts);

            bool write_task_info_to_db(std::string taskid);

            bool read_task_info_from_db(std::vector<std::string> &vec);

        protected:

            std::shared_ptr<leveldb::DB> m_req_training_task_db;

        };

    }

}

