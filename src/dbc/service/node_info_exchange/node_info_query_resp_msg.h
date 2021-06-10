/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : node_info_query_resp_msg.h
* description       : 
* date              : 2018/6/20
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
        class node_info_query_resp_msg
        {
        public:
            node_info_query_resp_msg(std::shared_ptr<message> msg);
            node_info_query_resp_msg();

            int32_t send();

            int32_t prepare(std::string o_node_id, std::string d_node_id, std::string session_id,
                            std::map<std::string, std::string> kvs);

            void set_path(std::vector<std::string> path);

            bool validate();

        private:
            std::shared_ptr<message> m_msg;
        };

    }
}
