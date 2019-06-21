/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ai_power_requestor_service.cpp
* description    :   ai_power_requestor_service
* date                  :   2018.01.28
* author            :   Bruce Feng
**********************************************************************************/
#include <cassert>
#include "server.h"
#include "api_call_handler.h"
#include "conf_manager.h"
#include "service_message_id.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "ip_validator.h"
#include <boost/exception/all.hpp>
#include "ai_power_requestor_service.h"
#include "id_generator.h"
#include "task_common_def.h"
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include "ai_crypter.h"

#include <sstream>

using namespace std;
using namespace matrix::core;
using namespace ai::dbc;


namespace ai
{
    namespace dbc
    {
        ai_power_requestor_service::ai_power_requestor_service()
        {

        }

        int32_t ai_power_requestor_service::init_conf()
        {
            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::service_init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;

            ret = m_req_training_task_db.init_db(env_manager::get_db_path());

            return ret;
        }


        void ai_power_requestor_service::init_subscription()
        {
            init_subscription_part1();

            init_subscription_part2();
        }

        void ai_power_requestor_service::init_invoker()
        {
            init_invoker_part1();

            init_invoker_part2();
        }


        void ai_power_requestor_service::init_subscription_part1()
        {
            //list training resp
            SUBSCRIBE_BUS_MESSAGE(LIST_TRAINING_RESP);

            //logs resp
            SUBSCRIBE_BUS_MESSAGE(LOGS_RESP);


            //forward binary message
            SUBSCRIBE_BUS_MESSAGE(BINARY_FORWARD_MSG);

        }

        void ai_power_requestor_service::init_invoker_part1()
        {
            invoker_type invoker;

            BIND_MESSAGE_INVOKER(LIST_TRAINING_RESP, &ai_power_requestor_service::on_list_training_resp);
            BIND_MESSAGE_INVOKER(LOGS_RESP, &ai_power_requestor_service::on_logs_resp);
            BIND_MESSAGE_INVOKER(BINARY_FORWARD_MSG, &ai_power_requestor_service::on_binary_forward);
        }

        void ai_power_requestor_service::init_timer()
        {
            m_timer_invokers[LIST_TRAINING_TIMER] = std::bind(&ai_power_requestor_service::on_list_training_timer, this, std::placeholders::_1);
            m_timer_invokers[TASK_LOGS_TIMER] = std::bind(&ai_power_requestor_service::on_logs_timer, this, std::placeholders::_1);
        }


        bool ai_power_requestor_service::precheck_msg(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "msg is nullptr";
                return false;
            }

            std::shared_ptr<msg_base> base = msg->content;


            if (!base)
            {
                LOG_ERROR << "containt is nullptr";
                return false;
            }

            if (id_generator().check_base58_id(base->header.nonce) != true)
            {
                LOG_ERROR << "nonce error ";
                return false;
            }

            if (id_generator().check_base58_id(base->header.session_id) != true)
            {
                LOG_ERROR << "ai power requster service on_list_training_resp. session_id error ";
                return false;
            }

            return true;
        }

        int32_t ai_power_requestor_service::on_list_training_resp(std::shared_ptr<message> &msg)
        {
            if(!precheck_msg(msg))
            {
                LOG_ERROR << "msg precheck fail";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(msg->content);
            if (!rsp_content)
            {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            if (use_sign_verify())
            {
                std::string task_status_msg="";
                for (auto t : rsp_content->body.task_status_list)
                {
                    task_status_msg = task_status_msg + t.task_id + boost::str(boost::format("%d") % t.status);
                }
                std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id + task_status_msg;
                if (! ai_crypto_util::verify_sign(sign_msg, rsp_content->header.exten_info, rsp_content->header.exten_info["origin_id"]))
                {
                    return E_DEFAULT;
                }
            }

            //get session
            std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
            if (nullptr == session)
            {
                LOG_ERROR << "ai power requester service get session null: " << rsp_content->header.session_id;

                //broadcast resp
                CONNECTION_MANAGER->send_resp_message(msg, msg->header.src_sid);

                return E_DEFAULT;
            }

            variables_map & vm = session->get_context().get_args();
            assert(vm.count("req_msg") > 0);
            assert(vm.count("task_ids") > 0);
            auto task_ids = vm["task_ids"].as< std::shared_ptr<std::unordered_map<std::string, int8_t>>>();
            std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();

            for (auto ts : rsp_content->body.task_status_list)
            {
                LOG_DEBUG << "recv list_training_resp: " << ts.task_id << " status: " << to_training_task_status_string(ts.status);
                task_ids->insert({ts.task_id, ts.status});
            }

            auto req_content = std::dynamic_pointer_cast<matrix::service_core::list_training_req>(req_msg->get_content());


            if (task_ids->size() < req_content->body.task_list.size())
            {
                LOG_DEBUG << "ai power requester service list task id less than " << req_content->body.task_list.size() << " and continue to wait";
                return E_SUCCESS;
            }

            auto vec_task_infos_to_show = vm["show_tasks"].as<std::shared_ptr<std::vector<ai::dbc::cmd_task_info>>>();
            //publish cmd resp
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
            COPY_MSG_HEADER(req_content,cmd_resp);
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto info : *vec_task_infos_to_show)
            {
                auto it = task_ids->find(info.task_id);
                if ((it != task_ids->end()))
                {
                    info.status = it->second;
                    {
                        // fetch old status value from db
                        ai::dbc::cmd_task_info task_info_in_db;
                        if(m_req_training_task_db.read_task_info_from_db(info.task_id, task_info_in_db))
                        {
                            if(task_info_in_db.status != info.status && info.status != task_unknown)
                            {
                                m_req_training_task_db.write_task_info_to_db(info);
                            }
                        }
                    }
                }

                cmd_task_status cts;
                cts.task_id = info.task_id;
                cts.status = info.status;
                cts.create_time = info.create_time;
                cts.description = info.description;
                cts.raw = info.raw;
                cts.pwd = info.pwd;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);

            //remember: remove timer
            this->remove_timer(session->get_timer_id());

            //remember: remove session
            session->clear();
            this->remove_session(session->get_session_id());

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_list_training_timer(std::shared_ptr<core_timer> timer)
        {
            //case: beware of initialization for 'goto'
            int32_t ret_code = E_SUCCESS;
            string session_id;
            std::shared_ptr<service_session> session;
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp;

            cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();

            assert(nullptr != timer);
            if (nullptr == timer)
            {
                LOG_ERROR << "null ptr of timer.";
                ret_code = E_NULL_POINTER;
                goto FINAL_PROC;
            }
            try
            {
                //get session
                session_id = timer->get_session_id();
                session = get_session(session_id);
                if (nullptr == session)
                {
                    LOG_ERROR << "ai power requester service list training timer get session null: " << session_id;
                    ret_code =  E_DEFAULT;
                    goto FINAL_PROC;
                }

                variables_map &vm = session->get_context().get_args();
                assert(vm.count("task_ids") > 0);
                assert(vm.count("show_tasks") > 0);
                auto task_ids = vm["task_ids"].as< std::shared_ptr<std::unordered_map<std::string, int8_t>>>();
                auto vec_task_infos_to_show = vm["show_tasks"].as<std::shared_ptr<std::vector<ai::dbc::cmd_task_info>>>();

                assert(vm.count("req_msg") > 0);
                std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();
                auto req_content = std::dynamic_pointer_cast<matrix::service_core::list_training_req>(req_msg->get_content());

                //publish cmd resp

                COPY_MSG_HEADER(req_content,cmd_resp);

                cmd_resp->result = E_SUCCESS;
                cmd_resp->result_info = "";

                for (auto info : *vec_task_infos_to_show)
                {
                    cmd_task_status cts;
                    cts.task_id = info.task_id;
                    cts.create_time = info.create_time;
                    cts.description = info.description;
                    cts.raw = info.raw;
                    auto it = task_ids->find(info.task_id);
                    if (it != task_ids->end())
                    {
                        cts.status = it->second;
                        //update to db
                        info.status = it->second;
                        //if (it->second & (task_stopped | task_successfully_closed | task_abnormally_closed | task_overdue_closed))
//                        if (info.status >= task_stopped)
//                        {
//                            write_task_info_to_db(info);
//                        }
//                        else
                        {
                            // fetch old status value from db, then update it
                            ai::dbc::cmd_task_info task_info_in_db;
                            if(m_req_training_task_db.read_task_info_from_db(info.task_id, task_info_in_db))
                            {
                                if(task_info_in_db.status != info.status && info.status != task_unknown)
                                {
                                    m_req_training_task_db.write_task_info_to_db(info);
                                }
                            }
                        }

                    }
                    else
                    {
                        cts.status = info.status;
                    }

                    cmd_resp->task_status_list.push_back(std::move(cts));
                }
            }
            catch (system_error &e)
            {
                LOG_ERROR << "error: " << e.what();
            }
            catch (...)
            {
                LOG_ERROR << "error: Unknown error.";
            }

FINAL_PROC:
            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
            if (session)
            {
                LOG_DEBUG << "ai power requester service list training timer time out remove session: " << session_id;
                session->clear();
                this->remove_session(session_id);
            }

            return ret_code;
        }



        int32_t ai_power_requestor_service::on_binary_forward(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "recv logs_resp but msg is nullptr";
                return E_DEFAULT;
            }


            std::shared_ptr<matrix::core::msg_forward> content = std::dynamic_pointer_cast<matrix::core::msg_forward>(msg->content);

            if(!content)
            {
                LOG_ERROR << "not a valid binary forward msg";
                return E_DEFAULT;
            }

            // support request message name end with "_req" postfix
            auto& msg_name = msg->content->header.msg_name;

            if (msg_name.substr(msg_name.size()-4) == std::string("_req"))
            {
                // add path
                msg->content->header.path.push_back(CONF_MANAGER->get_node_id());

                CONNECTION_MANAGER->broadcast_message(msg);
            }
            else
            {
                CONNECTION_MANAGER->send_resp_message(msg);
            }

            return E_SUCCESS;

        }


        int32_t ai_power_requestor_service::on_logs_resp(std::shared_ptr<message> &msg)
        {

            if(!precheck_msg(msg))
            {
                LOG_ERROR << "msg precheck fail";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::logs_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::logs_resp>(msg->content);
            if (!rsp_content)
            {
                LOG_ERROR << "recv logs_resp but ctn is nullptr";
                return E_DEFAULT;
            }
            
            std::string sign_msg = rsp_content->body.log.peer_node_id + rsp_content->header.nonce + rsp_content->header.session_id + rsp_content->body.log.log_content;
            if (! ai_crypto_util::verify_sign(sign_msg, rsp_content->header.exten_info, rsp_content->body.log.peer_node_id))
            {
                LOG_ERROR << "fake message. " << rsp_content->header.exten_info["origin_id"];
                return E_DEFAULT;
            }

            //get session
            std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
            if (nullptr == session)         //not self or time out, try broadcast
            {
                LOG_DEBUG << "ai power requestor service get session null: " << rsp_content->header.session_id;

                //broadcast resp
                CONNECTION_MANAGER->send_resp_message(msg);

                return E_SUCCESS;
            }

            std::shared_ptr<ai::dbc::cmd_logs_resp> cmd_resp = std::make_shared<ai::dbc::cmd_logs_resp>();
            COPY_MSG_HEADER(rsp_content,cmd_resp);

            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            //just support single machine + multi GPU now
            cmd_peer_node_log log;
            log.peer_node_id = rsp_content->body.log.peer_node_id;

            // jimmy: decrypt log_content
            ai_ecdh_cipher cipher;
            cipher.m_pub = rsp_content->header.exten_info["ecdh_pub"];
            if (!cipher.m_pub.empty())
            {
                LOG_DEBUG << " decrypt logs content ";
                cipher.m_data = std::move(rsp_content->body.log.log_content);

                ai_ecdh_crypter crypter(static_cast<secp256k1_context *>(get_context_sign()));

                CKey key;
                if (!ai_crypto_util::get_local_node_private_key(key))
                {
                    LOG_ERROR << " fail to get local node's private key";
                }
                else
                {
                    if (!crypter.decrypt(cipher, key, log.log_content))
                    {
                        LOG_ERROR << "fail to decrypt log content";
                    }
                }
            }
            else
            {
                log.log_content = std::move(rsp_content->body.log.log_content);
            }


            cmd_resp->peer_node_logs.push_back(std::move(log));

            // support sub_operation: download training result
            {
                auto vm = session->get_context().get_args();
                if (vm.count("sub_op"))
                {
                    auto op = vm["sub_op"].as<std::string>();
                    cmd_resp->sub_op = op;
                }

                if (vm.count("dest_folder"))
                {
                    auto df = vm["dest_folder"].as<std::string>();
                    cmd_resp->dest_folder = df;
                }

                if(vm.count("task_id"))
                {
                    auto ti = vm["task_id"].as<std::string>();
                    cmd_resp->task_id = ti;
                }
            }



            // try to get pwd from logs
            {

                std::string pwd = cmd_resp->get_value_from_log("pwd:");
                if (!pwd.empty())
                {
                    ai::dbc::cmd_task_info task_info_in_db;
                    if (!m_req_training_task_db.read_task_info_from_db(cmd_resp->task_id, task_info_in_db))
                    {
                        LOG_ERROR << "fail to get task info from db:  " << cmd_resp->task_id;
                    }
                    else
                    {
                        if (pwd != task_info_in_db.pwd)
                        {
                            task_info_in_db.__set_pwd(pwd);
                            m_req_training_task_db.write_task_info_to_db(task_info_in_db);
                        }
                    }
                }
            }


            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);

            //remember: remove timer
            this->remove_timer(session->get_timer_id());

            //remember: remove session
            session->clear();
            this->remove_session(session->get_session_id());

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_logs_timer(std::shared_ptr<core_timer> timer)
        {
            assert(nullptr != timer);

            //get session
            const string &session_id = timer->get_session_id();
            std::shared_ptr<service_session> session = get_session(session_id);
            if (nullptr == session)
            {
                LOG_ERROR << "ai power requestor service logs timer get session null: " << session_id;
                return E_DEFAULT;
            }

            variables_map &vm = session->get_context().get_args();
            assert(vm.count("req_msg") > 0);
            std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();
            auto req_content = std::dynamic_pointer_cast<matrix::service_core::logs_req>(req_msg->get_content());


            //publish cmd resp
            std::shared_ptr<ai::dbc::cmd_logs_resp> cmd_resp = std::make_shared<ai::dbc::cmd_logs_resp>();
            COPY_MSG_HEADER(req_content,cmd_resp);

            cmd_resp->header.__set_session_id(session_id );
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "get log time out";

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);

            //remember: remove session
            LOG_DEBUG << "ai power requestor service get logs timer time out remove session: " << session_id;
            session->clear();
            this->remove_session(session_id);

            return E_SUCCESS;
        }

    }//service_core
}
