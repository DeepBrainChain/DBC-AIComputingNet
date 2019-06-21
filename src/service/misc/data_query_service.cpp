/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : data_query_service.cpp
* description       :
* date              : 2018/6/13
* author            : Jimmy Kuang
**********************************************************************************/

#include "data_query_service.h"
#include "server.h"
#include "api_call_handler.h"

#include "service_message_id.h"
#include "matrix_types.h"

#include "node_info_query_req_msg.h"
#include "node_info_query_resp_msg.h"

#include "pc_service_module.h"
#include "node_info_collection.h"
#include "service_info_collection.h"
#include "service_broadcast_req_msg.h"
#include "service_name.h"

#include "service_topic.h"
#include <ctime>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>

#include "ai_crypter.h"

using namespace matrix::service_core;

namespace service
{
    namespace misc
    {

        #define NODE_INFO_QUERY_TIMER                       "node_info_query_timer"
        #define TIMER_INTERVAL_NODE_INFO_COLLECTION           (5*1000)

        #define NODE_INFO_COLLECTION_TIMER                  "node_info_collection_timer"
        //#define TIMER_INTERVAL_SERVICE_BROADCAST              (30*1000)
        #define SERVICE_BROADCAST_TIMER                     "service_broadcast_timer"

        data_query_service::data_query_service()
        {

        }

        /**
         * init
         * @param options
         * @return
         */
        int32_t data_query_service::init(bpo::variables_map &options)
        {
            LOG_DEBUG << "data_query_service::int";

            m_own_node_id = CONF_MANAGER->get_node_id();

            m_service_info_collection.init(m_own_node_id);

            m_is_computing_node = false;

            if (options.count(SERVICE_NAME_AI_TRAINING))
            {
                m_is_computing_node = true;

                std::string fn = env_manager::get_home_path().generic_string() + "/.dbc_bash.sh";
                auto rtn = m_node_info_collection.init(fn);
                if (rtn != E_SUCCESS)
                {
                    return rtn;
                }

                cfg_own_node(options, m_own_node_id);
            }



            return service_module::init(options);
        }

        /**
         * init timer
         */
        void data_query_service::init_timer()
        {
            m_timer_invokers[NODE_INFO_COLLECTION_TIMER] = std::bind(&data_query_service::on_timer_node_info_collection,
                                                                     this, std::placeholders::_1);
            add_timer(NODE_INFO_COLLECTION_TIMER, TIMER_INTERVAL_NODE_INFO_COLLECTION);

            m_timer_invokers[SERVICE_BROADCAST_TIMER] = std::bind(&data_query_service::on_timer_service_broadcast, this,
                                                                  std::placeholders::_1);
            //add_timer(SERVICE_BROADCAST_TIMER, TIMER_INTERVAL_SERVICE_BROADCAST);
            add_timer(SERVICE_BROADCAST_TIMER, CONF_MANAGER->get_timer_service_broadcast_in_second() * 1000 );


            m_timer_invokers[NODE_INFO_QUERY_TIMER] = std::bind(&data_query_service::on_guard_timer_expired_for_node_info_query, this, std::placeholders::_1);

        }

        /**
         * add own node info into service info collection
         * @param options
         * @param own_id
         */
        void data_query_service::cfg_own_node(bpo::variables_map &options, std::string own_id)
        {
            node_service_info info;
            info.service_list.push_back(SERVICE_NAME_AI_TRAINING);


            if (options.count("name"))
            {
                auto name_str = options["name"].as<std::string>();
                if(name_str.length() > MAX_NAME_STR_LENGTH)
                {
                    info.__set_name(name_str.substr(0,MAX_NAME_STR_LENGTH));
                }
                else
                {
                    info.__set_name(name_str);
                }
            }
            else
            {
                info.__set_name("anonymous");
            }

            auto t = std::time(nullptr);
            if ( t != (std::time_t)(-1))
            {
                info.__set_time_stamp(t);
            }

            std::map<std::string, std::string> kvs;

            const char* ATTRS[] = {
                    "gpu",
                    "gpu_usage",
                    "state",
                    "version",
                    "startup_time",
                    "gpu_state"
            };

            int num_of_attrs = sizeof(ATTRS)/sizeof(char*);

            for(int i=0; i< num_of_attrs; i++)
            {
                auto k = ATTRS[i];
                kvs[k] = m_node_info_collection.get(k);
            }

            info.__set_kvs(kvs);

            m_service_info_collection.add(own_id, info);
        }

        /**
         *
         * @param timer
         * @return
         */
        int32_t data_query_service::on_timer_node_info_collection(std::shared_ptr<matrix::core::core_timer> timer)
        {
            if(m_is_computing_node)
                m_node_info_collection.refresh();

            return E_SUCCESS;
        }


        /**
         * add topic subscription
         */
        void data_query_service::init_subscription()
        {
            LOG_DEBUG << "data_query_service::init_subscription";

            SUBSCRIBE_BUS_MESSAGE(typeid(ai::dbc::cmd_show_req).name());
            SUBSCRIBE_BUS_MESSAGE(SHOW_REQ);
            SUBSCRIBE_BUS_MESSAGE(SHOW_RESP);
            SUBSCRIBE_BUS_MESSAGE(SERVICE_BROADCAST_REQ);

            SUBSCRIBE_BUS_MESSAGE(typeid(service::get_task_queue_size_resp_msg).name());
        }

        /**
         * add topic handler
         */
        void data_query_service::init_invoker()
        {
            LOG_DEBUG << "data_query_service::init_invoker";

            invoker_type invoker;

            invoker = std::bind(&data_query_service::on_net_show_req, this, std::placeholders::_1);
            m_invokers.insert({SHOW_REQ, {invoker}});

            invoker = std::bind(&data_query_service::on_net_show_resp, this, std::placeholders::_1);
            m_invokers.insert({SHOW_RESP, {invoker}});

            invoker = std::bind(&data_query_service::on_cli_show_req, this, std::placeholders::_1);
            m_invokers.insert({typeid(ai::dbc::cmd_show_req).name(), {invoker}});

            invoker = std::bind(&data_query_service::on_net_service_broadcast_req, this, std::placeholders::_1);
            m_invokers.insert({SERVICE_BROADCAST_REQ, {invoker}});

            BIND_MESSAGE_INVOKER(typeid(service::get_task_queue_size_resp_msg).name(), &data_query_service::on_get_task_queue_size_resp);
        }


        /**
         * callback when show req is received from command line
         * @param msg the show requrest content
         * @return error code
         */
        int32_t data_query_service::on_cli_show_req(std::shared_ptr<message> &msg)
        {
            LOG_DEBUG << "data_query_service::on_cli_show_req";

            auto cmd_resp = std::make_shared<ai::dbc::cmd_show_resp>();
            auto content = msg->get_content();
            auto req = std::dynamic_pointer_cast<ai::dbc::cmd_show_req>(content);
            COPY_MSG_HEADER(req,cmd_resp);

            if (!req || !content)
            {
                cmd_resp->error("fatal: null ptr");
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            if (req->op == ai::dbc::OP_SHOW_SERVICE_LIST)
            {
                cmd_resp->filter = req->filter;
                cmd_resp->sort = req->sort;
                cmd_resp->op = req->op;
                cmd_resp->id_2_services = m_service_info_collection.get(cmd_resp->filter, cmd_resp->sort, req->num_lines);

                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);

                return E_SUCCESS;

            }
            else if (req->op == ai::dbc::OP_SHOW_NODE_INFO)
            {
                if (id_generator().check_node_id(req->d_node_id) != true)
                {

                    cmd_resp->error("invalid node id: " + req->d_node_id);

                    TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);
                    return E_DEFAULT;
                }

                if(req->d_node_id == req->o_node_id)
                {

                    cmd_resp->op = ai::dbc::OP_SHOW_NODE_INFO;

                    std::map<std::string, std::string> kvs;


                    std::vector<std::string> tmp_keys;
                    if (req->keys.size() == 1 && req->keys[0]=="all")
                    {
                        tmp_keys = m_node_info_collection.get_all_attributes();
                    }
                    else
                    {
                        tmp_keys = req->keys;
                    }

                    for (auto &k: tmp_keys)
                    {
                        kvs[k] = m_node_info_collection.get(k);
                    }
                    cmd_resp->kvs = kvs;

                    //cmd_resp->error("do not query local node");

                    TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);
                    return E_SUCCESS;

                }

                auto q = std::make_shared<node_info_query_req_msg>();
                std::string sesssion_id = req->header.session_id;

                if (req->keys.size() == 1 && req->keys[0]=="all")
                {
                    auto tmp_keys = m_node_info_collection.get_all_attributes();
                    q->prepare(req->o_node_id, req->d_node_id, tmp_keys,sesssion_id);
                }
                else
                {
                    q->prepare(req->o_node_id, req->d_node_id, req->keys,sesssion_id);
                }

                int32_t rtn = create_data_query_session(q->get_session_id(), q );
                if( rtn != E_SUCCESS)
                {

                    cmd_resp->error("internal error");

                    TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);
                    return E_DEFAULT;
                }

                q->send();
            }
            else
            {
                cmd_resp->error("unknown operation " + std::to_string(req->op));
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        /**
         * callback when show req from network is received
         * @param msg
         * @return
         */
        int32_t data_query_service::on_net_show_req(std::shared_ptr<message> &msg)
        {
            //LOG_DEBUG << "data_query_service::on_net_show_req";

            std::shared_ptr<show_req> content = std::dynamic_pointer_cast<show_req>(msg->get_content());

            if (content == nullptr ||  !node_info_query_req_msg(msg).validate())
            {
                LOG_DEBUG << "invalid msg";
                return E_DEFAULT;
            }


            auto o_node_id = content->body.o_node_id;
            auto d_node_id = content->body.d_node_id;

            std::string sign_msg = content->header.nonce 
                                     + content->header.session_id
                                     +d_node_id+boost::algorithm::join(content->body.keys, "");
            if (! ai_crypto_util::verify_sign(sign_msg, content->header.exten_info,o_node_id))
            {
                LOG_ERROR << "fake message. " << o_node_id;
                return E_DEFAULT;
            }

            if (d_node_id == m_own_node_id)
            {
                // destination node
                LOG_DEBUG << "data_query_service::on_net_show_req destination reached";

                node_info_query_resp_msg r;
                std::map<std::string, std::string> kvs;

                for (auto &k: content->body.keys)
                {
                    kvs[k] = m_node_info_collection.get(k);
                }

                r.prepare(m_own_node_id, o_node_id, content->header.session_id, kvs);
                r.set_path(content->header.path);

                LOG_DEBUG << "data_query_service::on_net_show_req send show_resp";

                r.send();

                return E_SUCCESS;
            }
            else if (o_node_id != m_own_node_id && d_node_id != m_own_node_id)
            {
                // middle node
                LOG_DEBUG << "data_query_service::on_net_show_req relay show_req to neighbor";

                content->header.path.push_back(m_own_node_id); //add this node id into path
                CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
            }
            else
            {
                // origin node
                LOG_ERROR << "data_query_service::on_net_show_req to origin node.";
            }

            return E_SUCCESS;

        }

        /**
         * create session for an ongoing data query:  add a guard timer and add one session record in cache
         * @param session_id
         * @param q
         * @return
         */
        int32_t data_query_service::create_data_query_session(std::string session_id, std::shared_ptr<node_info_query_req_msg> q)
        {
            //guard timer
            uint32_t timer_id = add_timer(NODE_INFO_QUERY_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(),
                                          ONLY_ONE_TIME, session_id);

            if (INVALID_TIMER_ID == timer_id)
            {
                LOG_ERROR << "fail to add timer NODE_INFO_QUERY_TIMER";
                return E_DEFAULT;
            }

            //service session
            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, session_id);

            //add to session
            int32_t ret = add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "data query service add session error: " << session->get_session_id();
                remove_timer(timer_id);
                return ret;
            }

            return ret;
        }

        /**
         * remove an ongoing data query session
         * @param session
         */
        void data_query_service::rm_data_query_session(std::shared_ptr<service_session> session)
        {
            if (nullptr == session)
            {
                return;
            }

            remove_timer(session->get_timer_id());
            session->clear();
            remove_session(session->get_session_id());
        }

        /**
         * callback when show resp is received from net
         * @param msg
         * @return
         */
        int32_t data_query_service::on_net_show_resp(std::shared_ptr<message> &msg)
        {
            //LOG_DEBUG << "data_query_service::on_net_show_resp";

            std::shared_ptr<show_resp> content = std::dynamic_pointer_cast<show_resp>(msg->get_content());

            if (content == nullptr ||  !node_info_query_resp_msg(msg).validate())
            {
                LOG_ERROR << "invalid msg";
                return E_DEFAULT;
            }

            auto o_node_id = content->body.o_node_id;
            auto d_node_id = content->body.d_node_id;

            std::string sign_msg = content->header.nonce + content->header.session_id+d_node_id 
                                     + boost::algorithm::join(content->body.kvs | boost::adaptors::map_values, "");
            if (! ai_crypto_util::verify_sign(sign_msg, content->header.exten_info,o_node_id))
            {
                LOG_ERROR << "fake message. " << o_node_id;
                return E_DEFAULT;
            }

            if (d_node_id == m_own_node_id)
            {
                // destination node
                LOG_DEBUG << "data_query_service::on_net_show_resp destination reached";
                std::shared_ptr<ai::dbc::cmd_show_resp> cmd_resp = std::make_shared<ai::dbc::cmd_show_resp>();
                COPY_MSG_HEADER(content,cmd_resp);
                //get session
                std::shared_ptr<service_session> session = get_session(content->header.session_id);
                if (nullptr == session)
                {
                    LOG_WARNING << "on_net_show_resp   " << content->header.session_id;
                    return E_NULL_POINTER;
                }

                cmd_resp->o_node_id = o_node_id;
                cmd_resp->d_node_id = d_node_id;
                cmd_resp->kvs = content->body.kvs;
                cmd_resp->op = ai::dbc::OP_SHOW_NODE_INFO;

                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);
                rm_data_query_session(session);

                return E_SUCCESS;
            }
            else if (o_node_id != m_own_node_id && d_node_id != m_own_node_id)
            {
                // middle node

                CONNECTION_MANAGER->send_resp_message(msg);
            }
            else
            {
                // origin node
                LOG_ERROR << "data_query_service::on_net_show_resp to origin node.";
            }

            return E_SUCCESS;

        }

        /**
         * callback when period timer event is raised: this event will trigger an service broadcast.
         * @param timer
         * @return
         */
        int32_t data_query_service::on_timer_service_broadcast(std::shared_ptr<matrix::core::core_timer> timer)
        {
            LOG_DEBUG << "data_query_service::on_timer_service_broadcast";

            auto s_map_size = m_service_info_collection.size();

            if (s_map_size == 0)
            {
                //LOG_DEBUG << "skip broadcast due to no valid service info in cache";
                return E_SUCCESS;
            }

            // update service_info_collection of own node
            std::string v = m_node_info_collection.get_gpu_short_desc();

            m_service_info_collection.update(m_own_node_id,"gpu", v);

            v = m_node_info_collection.get("state");
            m_service_info_collection.update(m_own_node_id,"state", v);

            v = m_node_info_collection.get("version");
            m_service_info_collection.update(m_own_node_id,"version", v);

            v = m_node_info_collection.get_gpu_usage_in_total();
            m_service_info_collection.update(m_own_node_id,"gpu_usage", v);

            //if(m_node_info_collection.is_honest_node())
            m_service_info_collection.update_own_node_time_stamp(m_own_node_id);

            m_service_info_collection.remove_unlived_nodes(CONF_MANAGER->get_timer_service_list_expired_in_second());


            auto s_map = m_service_info_collection.get_change_set();
            if(s_map.size())
            {
                // broadcast
                service_broadcast_req_msg req;

                int32_t rtn = req.prepare(s_map);
                if (E_SUCCESS != rtn)
                {
                    LOG_ERROR << "data_query_service::on_timer_service_broadcast fail: " << rtn;
                    return rtn;
                }

                req.send();

            }

            return E_SUCCESS;
        }

        /**
         * callback when service broadcast request message is received from net.
         * @param msg
         * @return
         */
        int32_t data_query_service::on_net_service_broadcast_req(std::shared_ptr<message> &msg)
        {

            LOG_DEBUG << "data_query_service::on_net_broadcast_req enter";

            std::shared_ptr<service_broadcast_req> content = std::dynamic_pointer_cast<service_broadcast_req>(
                    msg->get_content());


            if (content == nullptr || !service_broadcast_req_msg(msg).validate())
            {
                LOG_ERROR << "invalid msg";
                return E_DEFAULT;
            }

            m_service_info_collection.add(content->body.node_service_info_map);

            return E_SUCCESS;

        }

        /**
         * callback for guard timer expired: It indicates a node_info_query_resp has not been received in time.
         * @param timer
         * @return
         */
        int32_t data_query_service::on_guard_timer_expired_for_node_info_query(std::shared_ptr<core_timer> timer)
        {
            int32_t ret_code = E_SUCCESS;
            string session_id;
            std::shared_ptr<service_session> session;
            std::shared_ptr<ai::dbc::cmd_show_resp> cmd_resp;

            if (nullptr == timer)
            {
                LOG_ERROR << "null ptr";
                return E_NULL_POINTER;
            }

            //get session
            session_id = timer->get_session_id();
            session = get_session(session_id);
            if (nullptr == session)
            {
                LOG_ERROR << "on_guard_timer_expired_for_node_info_query get session null: " << session_id;
                return E_NULL_POINTER;
            }

            cmd_resp = std::make_shared<ai::dbc::cmd_show_resp>();
            cmd_resp->header.__set_session_id( session_id );
            cmd_resp->error("time out");
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_show_resp).name(), cmd_resp);

            if (session)
            {
                session->clear();
                remove_session(session_id);
            }

            return ret_code;
        }


        /**
         * callback when topic "get_task_queue_size_resp" is received. This is a internal topic between ai_power_provider and data_query.
         * @param msg
         * @return
         */
        int32_t data_query_service::on_get_task_queue_size_resp(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<service::get_task_queue_size_resp_msg> resp =
                    std::dynamic_pointer_cast<service::get_task_queue_size_resp_msg>(
                    msg);

            if (nullptr == resp)
            {
                LOG_ERROR<< "on_get_task_queue_size_resp null ptr";
                return E_NULL_POINTER;
            }

            LOG_DEBUG << "on_get_task_queue_size_resp: task_queue_size=" << std::to_string(resp->get_task_size());
            m_node_info_collection.set("state", std::to_string(resp->get_task_size()));
            m_node_info_collection.set("gpu_state", resp->get_gpu_state());

            return E_SUCCESS;
        }

    }
}