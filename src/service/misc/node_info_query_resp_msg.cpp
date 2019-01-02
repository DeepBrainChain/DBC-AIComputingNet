/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : node_info_query_resp_msg.cpp
* description       : 
* date              : 2018/6/20
* author            : Jimmy Kuang
**********************************************************************************/



#include "node_info_query_resp_msg.h"
#include "matrix_types.h"
#include "log.h"
#include "server.h"
#include "id_generator.h"
#include "service_message_id.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>

using namespace matrix::service_core;

namespace service
{
    namespace misc
    {

#define MAX_NODE_INFO_KEYS_NUM 15
#define MAX_NODE_INFO_VALUE_LEN 8192

        node_info_query_resp_msg::node_info_query_resp_msg():
        m_msg(nullptr)
        {

        }

        node_info_query_resp_msg::node_info_query_resp_msg(std::shared_ptr<message> msg):
        m_msg(msg)
        {

        }

        int32_t node_info_query_resp_msg::send()
        {
            if (!m_msg)
            {
                return E_NULL_POINTER;
            }

            auto content = std::dynamic_pointer_cast<show_resp>(m_msg->get_content());
            if (!content)
            {
                return E_NULL_POINTER;
            }

            CONNECTION_MANAGER->send_resp_message(m_msg);

            return E_SUCCESS;
        }


        int32_t node_info_query_resp_msg::prepare(std::string o_node_id, std::string d_node_id, std::string session_id,
                              std::map<std::string, std::string> kvs)
        {
            auto msg = std::make_shared<message>();
            auto content = std::make_shared<show_resp>();

            //header
            content->header.__set_magic(CONF_MANAGER->get_net_flag());
            content->header.__set_msg_name(SHOW_RESP);
            content->header.__set_nonce(id_generator().generate_nonce());
            content->header.__set_session_id(session_id);

            //body
            content->body.__set_o_node_id(o_node_id);
            content->body.__set_d_node_id(d_node_id);
            content->body.__set_kvs(kvs);
            
            std::string message = content->header.nonce + content->header.session_id+d_node_id 
                                     + boost::algorithm::join(kvs | boost::adaptors::map_values, "");
            std::map<std::string, std::string> exten_info;
            if (E_SUCCESS != extra_sign_info(message,exten_info))
            {
                return E_DEFAULT;
            }
            content->header.__set_exten_info(exten_info);

            msg->set_content(content);
            msg->set_name(content->header.msg_name);

            m_msg = msg;

            return E_SUCCESS;
        }

        void node_info_query_resp_msg::set_path(std::vector<std::string> path)
        {
            if (m_msg)
            {
                auto content = m_msg->get_content();
                if (content)
                {
                    content->header.__set_path(path);
                }
            }
        }

        bool node_info_query_resp_msg::validate()
        {
            if(m_msg == nullptr)
            {
                LOG_WARNING << "msg is nullptr ";
                return false;
            }

            std::shared_ptr<show_resp> content = std::dynamic_pointer_cast<show_resp>(m_msg->get_content());

            if (nullptr == content)
            {
                LOG_WARNING << "content is nullptr ";
                return false;
            }


            if(content->body.kvs.size() > MAX_NODE_INFO_KEYS_NUM)
            {
                LOG_WARNING << "too many keys " << content->body.kvs.size();
                return false;
            }

            for (auto& kv: content->body.kvs)
            {
                if (kv.second.size() > MAX_NODE_INFO_VALUE_LEN)
                {
                    LOG_WARNING << "string over length " << kv.second.size();
                    kv.second = kv.second.substr(0,MAX_NODE_INFO_VALUE_LEN);
                }
            }


            auto o_node_id = content->body.o_node_id;
            auto d_node_id = content->body.d_node_id;

            if (!id_generator().check_base58_id(o_node_id))
            {
                LOG_WARNING << "invalid o_node_id ";
                return false;
            }

            if (!id_generator().check_base58_id(d_node_id))
            {
                LOG_WARNING << "invalid d_node_id ";
                return false;
            }



            return true;

        }

    }
}