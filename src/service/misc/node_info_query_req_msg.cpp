/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : query.cpp
* description       : 
* date              : 2018/6/13
* author            : Jimmy Kuang
**********************************************************************************/

#include "node_info_query_req_msg.h"
#include "matrix_types.h"
#include "log.h"
#include "server.h"
#include "id_generator.h"
#include "service_message_id.h"
#include "node_info_collection.h"
#include <boost/algorithm/string/join.hpp>

#include "ai_crypter.h"

using namespace matrix::service_core;

namespace service
{
    namespace misc
    {
#define MAX_NODE_INFO_KEYS_NUM 30
#define MAX_NODE_INFO_KEY_LENGTH 30

        node_info_query_req_msg::node_info_query_req_msg():
            m_msg(nullptr)
        {

        }

        node_info_query_req_msg::node_info_query_req_msg(std::shared_ptr<message> msg):
            m_msg(msg)
        {

        }

        /**
        * prepare query
        * @param o_node_id the id of origin node
        * @param d_node_id the id of target node
        * @param k the name of the attributes
        */
        void node_info_query_req_msg::prepare(std::string o_node_id, std::string d_node_id, std::vector<std::string>
            keys,std::string   session_id)
        {
            auto msg = std::make_shared<message>();
            auto content = std::make_shared<matrix::service_core::show_req>();

            //header
            content->header.__set_magic(CONF_MANAGER->get_net_flag());
            content->header.__set_msg_name(SHOW_REQ);
            content->header.__set_nonce(id_generator().generate_nonce());
            content->header.__set_session_id(session_id);

            //body
            content->body.__set_o_node_id(o_node_id);
            content->body.__set_d_node_id(d_node_id);

            content->body.__set_keys(keys);

            std::vector<std::string> path;
            path.push_back(o_node_id);
            content->header.__set_path(path);

            std::string sign_msg = content->header.nonce 
                           + content->header.session_id+d_node_id
                           +boost::algorithm::join(content->body.keys, "");
            std::map<std::string, std::string> exten_info;
            if (E_SUCCESS != ai_crypto_util::extra_sign_info(sign_msg, exten_info))
            {
                return;
            }
            content->header.__set_exten_info(exten_info);

            msg->set_content(content);
            msg->set_name(content->header.msg_name);

            m_msg = msg;
        }


        /**
        * send query to a target node
        * @param msg the message to be sent out
        * @return error code
        */
        int32_t node_info_query_req_msg::send()
        {

            CONNECTION_MANAGER->broadcast_message(m_msg);

            return E_SUCCESS;
        }

        /**
         * get session id
         * @return session id
         */
        std::string node_info_query_req_msg::get_session_id()
        {
            return m_msg->content->header.session_id;
        }


        bool node_info_query_req_msg::validate()
        {
            if(m_msg == nullptr)
            {
                LOG_WARNING << "msg is nullptr ";
                return false;
            }

            std::shared_ptr<show_req> content = std::dynamic_pointer_cast<show_req>(m_msg->get_content());

            if (nullptr == content)
            {
                LOG_WARNING << "content is nullptr ";
                return false;
            }

            if(content->body.keys.size() == 0 )
            {
                LOG_WARNING << "no key exist";
                return false;
            }

            if(content->body.keys.size() > MAX_NODE_INFO_KEYS_NUM)
            {
                LOG_WARNING << "too many keys " << content->body.keys.size();
                return false;
            }

            for (auto& k: content->body.keys)
            {
                if (k.size() > MAX_NODE_INFO_KEY_LENGTH)
                {
                    LOG_WARNING << "string over length " << k.size();
                    return false;
                }
            }


            std::string o_node_id = content->body.o_node_id;
            if (!id_generator().check_base58_id(o_node_id))
            {
                LOG_WARNING << "invalid o_node_id ";
                return false;
            }

            std::string d_node_id = content->body.d_node_id;
            if (!id_generator().check_base58_id(d_node_id))
            {
                LOG_WARNING << "invalid d_node_id ";
                return false;
            }


            return true;

        }

    }
}
