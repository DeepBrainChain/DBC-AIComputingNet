/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : service_info_collection.h
* description       : 
* date              : 2018/6/19
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <mutex>
#include <list>
#include <map>
#include <string>
#include <boost/serialization/singleton.hpp>

#include "matrix_types.h"

#include "filter/simple_expression.h"

using namespace boost::serialization;
using namespace matrix::service_core;

namespace service
{
    namespace misc
    {
        typedef std::map <std::string, node_service_info> service_info_map;

        class service_info_collection //: public singleton<service_info_collection>
        {
        public:
            service_info_collection();
            void init(std::string );
            void add(std::string id, node_service_info s);
            void add(service_info_map);
            service_info_map get(std::string filter);
            void update_own_node_time_stamp(std::string own_node_id);
            void remove_unlived_nodes(int32_t time_in_second);

            int32_t size();
            void update(std::string node_id, std::string k, std::string v);

            void reset_change_set();
            service_info_map& get_change_set();


        private:
            bool check(expression& e, std::string filter, std::string node_id, node_service_info& s_info);
            std::string get_gpu_type(std::string s);
            std::string get_gpu_num(std::string s);
            std::string to_string(std::vector<std::string> in);


        public:
            enum {
                MAX_STORED_SERVICE_INFO_NUM = 10000
            };
        private:

            //std::mutex m_mutex;
            service_info_map m_id_2_info;
            service_info_map m_change;
            std::list<std::string> m_id_list;
            std::string m_own_node_id;

        };

    }

}
