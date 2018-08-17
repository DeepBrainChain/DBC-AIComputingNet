/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_client.h
* description    :   container client for definition
* date                  :   2018.9.15
* author            :   Regulus
**********************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "ai_db_types.h"
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <set>

namespace bp = boost::process;
namespace ai
{
    namespace dbc
    { 
        enum pull_state
        {
            PREPARE,
            PULLING,
            PULLING_SUCCESS,
            PULLING_ERROR
        };

        class image_manager
        {
        public:

            image_manager();

            ~image_manager() = default;

            int32_t start_pull(const std::string & image_name);
            int32_t terminate_pull();
            pull_state check_pull_state();
            uint32_t get_timer_id() { return m_image_timer_id; }
            void set_timer_id(uint32_t image_timer_id) { m_image_timer_id = image_timer_id; }
            void add_task(std::string task_id) { m_pulling_tasks.insert(task_id); }
            void rm_task(std::string task_id) { m_pulling_tasks.erase(task_id); }
            uint32_t get_task_count() { return m_pulling_tasks.size();}
        private:
            std::shared_ptr<bp::child> m_c_pull_image;
            uint32_t m_image_timer_id = 0;
            std::set<std::string> m_pulling_tasks;
        };
    }
}
