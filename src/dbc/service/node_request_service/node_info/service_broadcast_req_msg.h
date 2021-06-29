/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : service_broadcast_req.h
* description       : 
* date              : 2018/6/19
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include "service_message.h"
#include "matrix_types.h"

#include "service_info_collection.h"

namespace service
{
    namespace misc
    {
        class service_broadcast_req_msg
        {
        public:
            service_broadcast_req_msg();
            service_broadcast_req_msg(std::shared_ptr<message>);
            int32_t prepare(service_info_map m);
            void send();

            bool validate();

        public:
            enum {
                MAX_NODE_INFO_MAP_SIZE = 64,
                MAX_SERVCIE_LIST_SIZE = 10,
                MAX_ATTR_STRING_LEN = 64,
                MAX_LONG_ATTR_STRING_LEN = 128,
                MAX_TIMESTAMP_DRIFT_IN_SECOND = 60,
                MAX_NDOE_INFO_KVS_SIZE = 10
            };
        private:
            std::shared_ptr<message> m_msg;
        };
    }
}