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

        class bash_interface
        {
        public:
            bash_interface();
            bool init(std::string fn, std::string text);
            std::string run(std::string arg);

        private:
            std::string m_fn;
        };

        class node_info_collection
        {
        public:
            node_info_collection();

            std::string get(std::string);

            void set(std::string k, std::string v);

            void refresh();

            int32_t init(std::string);

            std::string get_gpu_usage_in_total();

            time_t get_node_startup_time();

            void set_node_startup_time();

            int32_t read_node_static_info(std::string fn);

            std::string get_gpu_short_desc();

        public:
            static std::vector<std::string> get_all_attributes();

        private:
            void generate_node_static_info(std::string path);

            std::string pretty_state(std::string v);

        private:
            std::map<std::string, std::string> m_kvs;

            bash_interface m_shell;
        };

    }

}