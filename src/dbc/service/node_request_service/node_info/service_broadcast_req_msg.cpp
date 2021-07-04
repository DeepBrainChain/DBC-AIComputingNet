/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : service_broadcast_req.cpp
* description       : 
* date              : 2018/6/19
* author            : Jimmy Kuang
**********************************************************************************/

#include "service_broadcast_req_msg.h"
#include "error.h"
#include "log.h"
#include "server.h"
#include <ctime>
#include "service_message_id.h"

using namespace matrix::service_core;

namespace service
{
    namespace misc
    {

        service_broadcast_req_msg::service_broadcast_req_msg(): m_msg(nullptr)
        {

        }

        service_broadcast_req_msg::service_broadcast_req_msg(std::shared_ptr<dbc::network::message> msg):m_msg(msg)
        {

        }

        int32_t service_broadcast_req_msg::prepare(service_info_map m)
        {
            LOG_DEBUG<<"service_broadcast_req::prepare";

            auto msg = std::make_shared<dbc::network::message>();
            auto content = std::make_shared<matrix::service_core::service_broadcast_req>();

            //header
            content->header.__set_magic(CONF_MANAGER->get_net_flag());
            content->header.__set_msg_name(SERVICE_BROADCAST_REQ);
            content->header.__set_nonce(dbc::create_nonce());
            content->header.__set_session_id("");

            //body
            content->body.__set_node_service_info_map(m);

//            LOG_DEBUG<<content->header;
//            LOG_DEBUG<<content->body;

            msg->set_content(content);
            msg->set_name(content->header.msg_name);

            m_msg = msg;



            return E_SUCCESS;
        }


        void service_broadcast_req_msg::send()
        {
            LOG_DEBUG<<"service_broadcast_req::send";

            CONNECTION_MANAGER->broadcast_message(m_msg);
        }


        bool service_broadcast_req_msg::validate()
        {
            if(m_msg == nullptr)
            {
                LOG_WARNING << "msg is nullptr ";
                return false;
            }

            std::shared_ptr<service_broadcast_req> content = std::dynamic_pointer_cast<service_broadcast_req>(m_msg->get_content());

            if (nullptr == content)
            {
                LOG_WARNING << "content is nullptr ";
                return false;
            }

            if(content->body.node_service_info_map.size() == 0)
            {
                LOG_WARNING << "empty service info map";
                return false;
            }

            if(content->body.node_service_info_map.size() > MAX_NODE_INFO_MAP_SIZE)
            {
                LOG_WARNING << "too many sevice info " << content->body.node_service_info_map.size();
                return false;
            }

            for (auto& kv: content->body.node_service_info_map)
            {
                if (kv.first.size() > MAX_ATTR_STRING_LEN)
                {
                    LOG_WARNING << "string over length " << kv.first.size();
                    return false;
                }

                if (kv.second.service_list.size() > MAX_SERVCIE_LIST_SIZE)
                {
                    LOG_WARNING << "service list over length " << kv.second.service_list.size();
                    return false;
                }

                if (kv.second.name.size() > MAX_ATTR_STRING_LEN)
                {
                    LOG_WARNING << "node name over length " << kv.second.name.size();
                    return false;
                }


//                auto now = std::time(nullptr);
//                if ( now != (std::time_t)(-1))
//                {
//                    if (kv.second.time_stamp > (now + MAX_TIMESTAMP_DRIFT_IN_SECOND))
//                    {
//                        LOG_WARNING << "timestamp drift out of range" ;
//                        return false;
//                    }
//                }

                if ( kv.second.kvs.size() > MAX_NDOE_INFO_KVS_SIZE)
                {
                    LOG_WARNING << "kvs over size" ;
                    return false;
                }

                for (auto kv2: kv.second.kvs)
                {
                    if (kv2.first.size() > MAX_ATTR_STRING_LEN)
                    {
                        return false;
                    }

                    if (kv2.second.size() > MAX_LONG_ATTR_STRING_LEN)
                    {
                        return false;
                    }
                }

            }

            return true;

        }
    }
}

