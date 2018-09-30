/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        container_message.h
* description    container message definition
* date                  2018.10.17
* author            Regulus
**********************************************************************************/

#pragma once

#include "common.h"
#include "container_message.h"
#include "oss_common_def.h"

namespace matrix
{
    namespace core
    {
        const int64_t DEFAULT_GET_TASK_CYTLE = 24*60*60;
        class idle_task_req
        {
        public:

            std::string mining_node_id;

            std::string time_stamp;

            std::string sign_type;

            std::string sign;

            std::string to_string();
        };

        class idle_task_resp
        {
        public:

            int64_t status = OSS_SUCCESS;

            std::string idle_task_id;
            std::string code_dir;
            std::string entry_file;
            std::string training_engine;
            std::string data_dir;
            std::string hyper_parameters;

            int64_t report_cycle = DEFAULT_GET_TASK_CYTLE;
            void from_string(const std::string & buf);
        };
    }

}