//
// Created by Jianmin Kuang on 2019-05-14.
//

#pragma once

#include <string>
#include <vector>

#include "ai_db_types.h"
#include <leveldb/db.h>

#include <boost/filesystem.hpp>

namespace ai
{
    namespace dbc
    {

        class ai_requestor_task_db
        {
        public:

            int32_t init_db(boost::filesystem::path task_db_path);

            bool write_task_info_to_db(ai::dbc::cmd_task_info &task_info);

            bool read_task_info_from_db(std::vector <ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status = 0);

            bool
            read_task_info_from_db(std::list <std::string> task_ids, std::vector <ai::dbc::cmd_task_info> &task_infos,
                                   uint32_t filter_status = 0);

            bool read_task_info_from_db(std::string task_id, ai::dbc::cmd_task_info &task_info);

            bool is_task_exist_in_db(std::string task_id);

            void delete_task(std::string task_id);

            std::string get_node_id_from_db(std::string task_id);

            void reset_task_status_to_db(std::string task_id);


        protected:

            std::shared_ptr<leveldb::DB> m_task_db;

        };
    }
}