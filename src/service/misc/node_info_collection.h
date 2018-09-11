/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : node_info_collection.h
* description       : 
* date              : 2018/6/14
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <mutex>
#include <map>
#include <string>
#include <vector>

#include <boost/serialization/singleton.hpp>
#include <time.h>

using namespace boost::serialization;


namespace service
{
    namespace misc
    {
        class node_info_collection //: public singleton<node_info_collection>
        {
        public:
            node_info_collection();
            std::string get(std::string);
            void set(std::string k, std::string v);

            void refresh();
            int32_t init(bool enable=true);

            std::vector<std::string> get_keys();
            bool is_honest_node();

            std::string get_gpu_usage_in_total();
            void set_query_sh(std::string fn="");

            time_t get_node_startup_time();
            void set_node_startup_time();

        private:
            bool generate_query_sh_file(std::string text);

            std::string query(std::string k);
            std::string get_one(std::string k);

            bool check_sh_file(std::string fn);

            void reset_node_info();


        private:
            std::map<std::string, std::vector<std::string>> m_node_2_services;
            std::map<std::string, std::string> m_kvs;

            //std::mutex m_mutex;
            std::vector<std::string> m_keys;

            std::string m_query_sh_file_name;

            bool m_enable;
            std::size_t m_sh_file_hash;
            bool m_honest;
        };

    }

}