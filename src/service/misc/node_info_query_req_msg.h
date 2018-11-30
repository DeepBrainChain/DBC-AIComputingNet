/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : query.h
* description       : 
* date              : 2018/6/13
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <string>
#include <vector>

#include "matrix_types.h"


namespace service
{
    namespace misc
    {
        /*
         *  Query the attributes from a target node
         */
        class node_info_query_req_msg
        {
        public:
            node_info_query_req_msg();
            node_info_query_req_msg(std::shared_ptr<message> msg);
            int32_t send();
            void prepare(std::string session_id,std::string o_node_id, std::string d_node_id, std::vector<std::string> key);
            std::string get_session_id();

            bool validate();

        private:
            std::shared_ptr<message> m_msg;
        };

        /**
         * session of all outstanding query
         */
        class node_info_query_sessions
        {
        public:
            static void add(std::string session_id);
            static void rm(std::string session_id);
            static bool is_valid_session(std::string session_id);

        private:
            static std::map <std::string, std::string> m_sessions; // outstanding session
        };



    }
}


