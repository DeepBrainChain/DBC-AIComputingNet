/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : server_info.h
* description       : 
* date              : 2018/6/12
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once


#include <vector>
#include <string>

namespace matrix
{
    namespace core
    {
        class server_info
        {
        public:
            server_info();
            void add_service_list(std::string s) {m_service_list.push_back(s);}
            std::vector<std::string>& get_service_list() { return m_service_list;}

            server_info& operator = (const server_info& b);

        private:
            std::vector<std::string> m_service_list;

        };
    }
}