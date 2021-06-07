//
// Created by Jianmin Kuang on 2019-05-14.
//

#pragma once

#include <string>
#include <unordered_map>

#include "ai_db_types.h"
#include <leveldb/db.h>

#include <boost/filesystem.hpp>

namespace ai
{
    namespace dbc
    {

        class ai_provider_task_db
        {
        public:

            int32_t init_db(boost::filesystem::path task_db_path, std::string db_name);

            std::shared_ptr<ai_training_task> load_idle_task();

            int32_t load_user_task(std::unordered_map<std::string, std::shared_ptr<ai_training_task>> & training_tasks);

            int32_t write_task_to_db(std::shared_ptr<ai_training_task> task);
            int32_t delete_task(std::shared_ptr<ai_training_task> task);


        protected:

            std::shared_ptr<leveldb::DB> m_task_db;

        };
    }
}