#include "node_info_message.h"
#include "matrix_types.h"
#include "log.h"
#include "server.h"
#include "crypt_util.h"
#include "service_message_id.h"
#include "node_info_collection.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/format.hpp>


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
            content->header.__set_nonce(dbc::create_nonce());
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
            std::string sign = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
            exten_info["sign"] = sign;
            exten_info["sign_algo"] = ECDSA;
            time_t cur = std::time(nullptr);
            exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();

            content->header.__set_exten_info(exten_info);

            msg->set_content(content);
            msg->set_name(SHOW_REQ);

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

            return true;

        }


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
            content->header.__set_nonce(dbc::create_nonce());
            content->header.__set_session_id(session_id);

            //body
            content->body.__set_o_node_id(o_node_id);
            content->body.__set_d_node_id(d_node_id);
            content->body.__set_kvs(kvs);

            std::string message = content->header.nonce + content->header.session_id+d_node_id
                                  + boost::algorithm::join(kvs | boost::adaptors::map_values, "");
            std::map<std::string, std::string> exten_info;
            std::string sign = dbc::sign(message, CONF_MANAGER->get_node_private_key());
            exten_info["sign"] = sign;
            exten_info["sign_algo"] = ECDSA;
            time_t cur = std::time(nullptr);
            exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();

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

            return true;

        }

    }
}

