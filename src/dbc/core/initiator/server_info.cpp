/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : node_info.cpp
* description       : 
* date              : 2018/6/12
* author            : Jimmy Kuang
**********************************************************************************/

#include "server_info.h"

namespace matrix
{
    namespace core
    {
        server_info::server_info()
        {
            m_service_list.clear();
        }

        server_info& server_info::operator = (const server_info& b)
        {
            m_service_list = b.m_service_list;
            return *this;
        }

    }

}