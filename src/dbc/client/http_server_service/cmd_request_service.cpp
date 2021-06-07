/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   cmd_request_service.cpp
* description    :   cmd_request_service
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
#include "cmd_request_service.h"
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

namespace ai {
    namespace dbc {
        int32_t cmd_request_service::service_init(bpo::variables_map &options) {
            int32_t ret = m_req_training_task_db.init_db(env_manager::get_db_path());
            return ret;
        }

        void cmd_request_service::init_subscription() {
            //list training resp
            SUBSCRIBE_BUS_MESSAGE(LIST_TRAINING_RESP);

            //logs resp
            SUBSCRIBE_BUS_MESSAGE(LOGS_RESP);

            //forward binary message
            SUBSCRIBE_BUS_MESSAGE(BINARY_FORWARD_MSG);

            //cmd start training
            SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_start_training_req).name());

            //cmd stop training
            SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_stop_training_req).name());

            //cmd list training
            SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_list_training_req).name());

            //cmd logs req
            SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_logs_req).name());

            //cmd clean task req
            SUBSCRIBE_BUS_MESSAGE(typeid(::cmd_task_clean_req).name());
        }

        void cmd_request_service::init_invoker() {
            invoker_type invoker;
            BIND_MESSAGE_INVOKER(typeid(::cmd_start_training_req).name(), &cmd_request_service::on_cmd_start_training_req);
            BIND_MESSAGE_INVOKER(typeid(::cmd_stop_training_req).name(), &cmd_request_service::on_cmd_stop_training_req);
            BIND_MESSAGE_INVOKER(typeid(::cmd_task_clean_req).name(), &cmd_request_service::on_cmd_task_clean);
            BIND_MESSAGE_INVOKER(typeid(::cmd_logs_req).name(), &cmd_request_service::on_cmd_logs_req);
            BIND_MESSAGE_INVOKER(typeid(::cmd_list_training_req).name(), &cmd_request_service::on_cmd_list_training_req);

            BIND_MESSAGE_INVOKER(LIST_TRAINING_RESP, &cmd_request_service::on_list_training_resp);
            BIND_MESSAGE_INVOKER(LOGS_RESP, &cmd_request_service::on_logs_resp);
            BIND_MESSAGE_INVOKER(BINARY_FORWARD_MSG, &cmd_request_service::on_binary_forward);
        }

        void cmd_request_service::init_timer() {
            m_timer_invokers[LIST_TRAINING_TIMER] = std::bind(&cmd_request_service::on_list_training_timer, this, std::placeholders::_1);
            m_timer_invokers[TASK_LOGS_TIMER] = std::bind(&cmd_request_service::on_logs_timer, this, std::placeholders::_1);
        }

        int32_t cmd_request_service::on_list_training_timer(std::shared_ptr<core_timer> timer) {
            string session_id;
            std::shared_ptr<service_session> session;
            std::shared_ptr<::cmd_list_training_resp> cmd_resp = std::make_shared<::cmd_list_training_resp>();

            if (nullptr == timer) {
                LOG_ERROR << "null ptr of timer.";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                return E_NULL_POINTER;
            }

            try {
                session_id = timer->get_session_id();
                session = get_session(session_id);
                if (nullptr == session) {
                    LOG_ERROR << "ai power requester service list training timer get session null: " << session_id;
                    TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                    return E_DEFAULT;
                }

                variables_map &vm = session->get_context().get_args();
                assert(vm.count("task_ids") > 0);
                assert(vm.count("show_tasks") > 0);
                auto task_ids = vm["task_ids"].as<std::shared_ptr<std::unordered_map<std::string, int8_t>>>();
                auto vec_task_infos_to_show = vm["show_tasks"].as<std::shared_ptr<std::vector<ai::dbc::cmd_task_info>>>();

                assert(vm.count("req_msg") > 0);
                std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();
                auto req_content = std::dynamic_pointer_cast<matrix::service_core::list_training_req>(req_msg->get_content());

                //publish cmd resp
                if (nullptr != req_content)
                    cmd_resp->header.__set_session_id(req_content->header.session_id);

                cmd_resp->result = E_SUCCESS;
                cmd_resp->result_info = "";

                for (auto info : *vec_task_infos_to_show) {
                    ::cmd_task_status cts;
                    cts.task_id = info.task_id;
                    cts.create_time = info.create_time;
                    cts.description = info.description;
                    cts.raw = info.raw;
                    auto it = task_ids->find(info.task_id);
                    if (it != task_ids->end()) {
                        cts.status = it->second;
                        //update to db
                        info.status = it->second;
                        // fetch old status value from db, then update it
                        ai::dbc::cmd_task_info task_info_in_db;
                        if (m_req_training_task_db.read_task_info_from_db(info.task_id, task_info_in_db)) {
                            if (task_info_in_db.status != info.status && info.status != task_unknown) {
                                m_req_training_task_db.write_task_info_to_db(info);
                            }
                        }
                    } else {
                        cts.status = info.status;
                    }

                    cmd_resp->task_status_list.push_back(std::move(cts));
                }
            }
            catch (system_error &e) {
                LOG_ERROR << "error: " << e.what();
            }
            catch (...) {
                LOG_ERROR << "error: Unknown error.";
            }

            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
            if (session) {
                LOG_DEBUG << "ai power requester service list training timer time out remove session: " << session_id;
                session->clear();
                this->remove_session(session_id);
            }

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_logs_timer(std::shared_ptr<core_timer> timer) {
            assert(nullptr != timer);
            const string &session_id = timer->get_session_id();
            std::shared_ptr<service_session> session = get_session(session_id);
            if (nullptr == session) {
                LOG_ERROR << "ai power requestor service logs timer get session null: " << session_id;
                return E_DEFAULT;
            }

            variables_map &vm = session->get_context().get_args();
            assert(vm.count("req_msg") > 0);
            std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();
            auto req_content = std::dynamic_pointer_cast<matrix::service_core::logs_req>(req_msg->get_content());

            std::shared_ptr<::cmd_logs_resp> cmd_resp = std::make_shared<::cmd_logs_resp>();
            if (nullptr != req_content)
                cmd_resp->header.__set_session_id(req_content->header.session_id);
            cmd_resp->header.__set_session_id(session_id);
            cmd_resp->result = E_DEFAULT;
            cmd_resp->result_info = "get log time out";

            TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);

            session->clear();
            this->remove_session(session_id);

            return E_SUCCESS;
        }

        bool cmd_request_service::precheck_msg(std::shared_ptr<message> &msg) {
            if (!msg) {
                LOG_ERROR << "msg is nullptr";
                return false;
            }

            std::shared_ptr<msg_base> base = msg->content;
            if (!base) {
                LOG_ERROR << "containt is nullptr";
                return false;
            }

            if (!id_generator::check_base58_id(base->header.nonce)) {
                LOG_ERROR << "nonce error ";
                return false;
            }

            if (!id_generator::check_base58_id(base->header.session_id)) {
                LOG_ERROR << "ai power requster service on_list_training_resp. session_id error ";
                return false;
            }

            return true;
        }

        int32_t cmd_request_service::on_list_training_resp(std::shared_ptr<message> &msg) {
            if (!precheck_msg(msg)) {
                LOG_ERROR << "msg precheck fail";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(
                    msg->content);
            if (!rsp_content) {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            if (use_sign_verify()) {
                std::string task_status_msg = "";
                for (auto t : rsp_content->body.task_status_list) {
                    task_status_msg = task_status_msg + t.task_id + boost::str(boost::format("%d") % t.status);
                }
                std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id + task_status_msg;
                if (!ai_crypto_util::verify_sign(sign_msg, rsp_content->header.exten_info, rsp_content->header.exten_info["origin_id"])) {
                    return E_DEFAULT;
                }
            }

            //get session
            std::shared_ptr<service_session> session = get_session(rsp_content->header.session_id);
            if (nullptr == session) {
                LOG_ERROR << "ai power requester service get session null: " << rsp_content->header.session_id;

                //broadcast resp
                CONNECTION_MANAGER->send_resp_message(msg, msg->header.src_sid);

                return E_DEFAULT;
            }

            variables_map &vm = session->get_context().get_args();
            assert(vm.count("req_msg") > 0);
            assert(vm.count("task_ids") > 0);
            auto task_ids = vm["task_ids"].as<std::shared_ptr<std::unordered_map<std::string, int8_t>>>();
            std::shared_ptr<message> req_msg = vm["req_msg"].as<std::shared_ptr<message>>();

            for (auto ts : rsp_content->body.task_status_list) {
                LOG_DEBUG << "recv list_training_resp: " << ts.task_id << " status: " << to_training_task_status_string(ts.status);
                task_ids->insert({ts.task_id, ts.status});
            }

            auto req_content = std::dynamic_pointer_cast<matrix::service_core::list_training_req>(req_msg->get_content());


            if (task_ids->size() < req_content->body.task_list.size()) {
                LOG_DEBUG << "ai power requester service list task id less than " << req_content->body.task_list.size() << " and continue to wait";
                return E_SUCCESS;
            }

            auto vec_task_infos_to_show = vm["show_tasks"].as<std::shared_ptr<std::vector<ai::dbc::cmd_task_info>>>();
            //publish cmd resp
            std::shared_ptr<::cmd_list_training_resp> cmd_resp = std::make_shared<::cmd_list_training_resp>();
            COPY_MSG_HEADER(req_content, cmd_resp);
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto info : *vec_task_infos_to_show) {
                auto it = task_ids->find(info.task_id);
                if ((it != task_ids->end())) {
                    info.status = it->second;
                    {
                        // fetch old status value from db
                        ai::dbc::cmd_task_info task_info_in_db;
                        if (m_req_training_task_db.read_task_info_from_db(info.task_id, task_info_in_db)) {
                            if (task_info_in_db.status != info.status && info.status != task_unknown) {
                                m_req_training_task_db.write_task_info_to_db(info);
                            }
                        }
                    }
                }

                ::cmd_task_status cts;
                cts.task_id = info.task_id;
                cts.status = info.status;
                cts.create_time = info.create_time;
                cts.description = info.description;
                cts.raw = info.raw;
                cts.pwd = info.pwd;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);

            //remember: remove timer
            this->remove_timer(session->get_timer_id());

            //remember: remove session
            session->clear();
            this->remove_session(session->get_session_id());

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_binary_forward(std::shared_ptr<message> &msg) {
            if (!msg) {
                LOG_ERROR << "recv logs_resp but msg is nullptr";
                return E_DEFAULT;
            }


            std::shared_ptr<matrix::core::msg_forward> content = std::dynamic_pointer_cast<matrix::core::msg_forward>(msg->content);

            if (!content) {
                LOG_ERROR << "not a valid binary forward msg";
                return E_DEFAULT;
            }

            // support request message name end with "_req" postfix
            auto &msg_name = msg->content->header.msg_name;

            if (msg_name.substr(msg_name.size() - 4) == std::string("_req")) {
                // add path
                msg->content->header.path.push_back(CONF_MANAGER->get_node_id());

                LOG_INFO << "broadcast_message binary forward msg";
                CONNECTION_MANAGER->broadcast_message(msg);
            } else {
                CONNECTION_MANAGER->send_resp_message(msg);
            }

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_logs_resp(std::shared_ptr<message> &msg) {
            if (!precheck_msg(msg)) {
                LOG_ERROR << "msg precheck fail";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::logs_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::logs_resp>(msg->content);
            if (!rsp_content) {
                LOG_ERROR << "recv logs_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            std::string sign_msg = rsp_content->body.log.peer_node_id + rsp_content->header.nonce + rsp_content->header.session_id +
                                   rsp_content->body.log.log_content;
            if (!ai_crypto_util::verify_sign(sign_msg, rsp_content->header.exten_info, rsp_content->body.log.peer_node_id)) {
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

            std::shared_ptr<::cmd_logs_resp> cmd_resp = std::make_shared<::cmd_logs_resp>();
            COPY_MSG_HEADER(rsp_content, cmd_resp);

            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            //just support single machine + multi GPU now
            ::cmd_peer_node_log log;
            log.peer_node_id = rsp_content->body.log.peer_node_id;

            // jimmy: decrypt log_content
            ai_ecdh_cipher cipher;
            cipher.m_pub = rsp_content->header.exten_info["ecdh_pub"];
            if (!cipher.m_pub.empty()) {
                LOG_DEBUG << " decrypt logs content ";
                cipher.m_data = std::move(rsp_content->body.log.log_content);

                ai_ecdh_crypter crypter(static_cast<secp256k1_context *>(get_context_sign()));

                CKey key;
                if (!ai_crypto_util::get_local_node_private_key(key)) {
                    LOG_ERROR << " fail to get local node's private key";
                } else {
                    if (!crypter.decrypt(cipher, key, log.log_content)) {
                        LOG_ERROR << "fail to decrypt log content";
                    }
                }
            } else {
                log.log_content = std::move(rsp_content->body.log.log_content);
            }

            cmd_resp->peer_node_logs.push_back(std::move(log));

            // support sub_operation: download training result
            {
                auto vm = session->get_context().get_args();
                if (vm.count("sub_op")) {
                    auto op = vm["sub_op"].as<std::string>();
                    cmd_resp->sub_op = op;
                }

                if (vm.count("dest_folder")) {
                    auto df = vm["dest_folder"].as<std::string>();
                    cmd_resp->dest_folder = df;
                }

                if (vm.count("task_id")) {
                    auto ti = vm["task_id"].as<std::string>();
                    cmd_resp->task_id = ti;
                }
            }

            // try to get pwd from logs
            {
                std::string pwd = cmd_resp->get_value_from_log("pwd:");
                if (!pwd.empty()) {
                    ai::dbc::cmd_task_info task_info_in_db;
                    if (!m_req_training_task_db.read_task_info_from_db(cmd_resp->task_id, task_info_in_db)) {
                        LOG_ERROR << "fail to get task info from db:  " << cmd_resp->task_id;
                    } else {
                        if (pwd != task_info_in_db.pwd) {
                            task_info_in_db.__set_pwd(pwd);
                            m_req_training_task_db.write_task_info_to_db(task_info_in_db);
                        }
                    }
                }
            }

            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);

            //remember: remove timer
            this->remove_timer(session->get_timer_id());

            //remember: remove session
            session->clear();
            this->remove_session(session->get_session_id());

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_cmd_start_training_req(std::shared_ptr<message> &msg)
        {
            LOG_INFO << "cmd_request_service => on_cmd_start_training_req";

            std::shared_ptr<base> msg_content = msg->get_content();
            std::shared_ptr<::cmd_start_training_req> cmd_req_msg = std::dynamic_pointer_cast<::cmd_start_training_req>(msg_content);

            std::shared_ptr<::cmd_start_training_resp> cmd_resp = std::make_shared<::cmd_start_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_info.task_id = "";
            cmd_resp->task_info.create_time = time(nullptr);
            cmd_resp->task_info.status = task_unknown;

            if (cmd_req_msg == nullptr)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_start_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            cmd_resp->header.__set_session_id(cmd_req_msg->header.session_id);

            auto task_req_msg = create_task_req_msg(cmd_req_msg->vm, cmd_resp->task_info);
            if (nullptr == task_req_msg)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = cmd_resp->task_info.result;
                TOPIC_MANAGER->publish<void>(typeid(::cmd_start_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            if (CONNECTION_MANAGER->broadcast_message(task_req_msg) != E_SUCCESS)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "submit error. pls check network";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_start_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            TOPIC_MANAGER->publish<void>(typeid(::cmd_start_training_resp).name(), cmd_resp);

            std::string code_dir = cmd_req_msg->vm["code_dir"].as<std::string>();
            if (code_dir == NODE_REBOOT)
            {
                LOG_DEBUG << "not serialize for reboot task";
            }
            else if ( code_dir == TASK_RESTART )
            {
                m_req_training_task_db.reset_task_status_to_db(cmd_resp->task_info.task_id);
            }
            else
            {
                std::string task_description;
                try
                {
                    std::vector<std::string> nodes = cmd_req_msg->vm["peer_nodes_list"].as<std::vector<std::string>>();
                    std::string node = nodes[0];
                    task_description = node;
                    cmd_resp->task_info.peer_nodes_list.push_back(node);
                }
                catch (const std::exception &e)
                {

                }

                task_description += " : " + cmd_req_msg->parameters["description"];
                cmd_resp->task_info.__set_description(task_description);

                auto msg_content = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(task_req_msg->content);
                if (msg_content != nullptr)
                {
                    std::ostringstream stream;
                    stream << msg_content->body;
                    cmd_resp->task_info.__set_raw(stream.str());
                }

                m_req_training_task_db.write_task_info_to_db(cmd_resp->task_info);
            }

            return E_SUCCESS;
        }

        std::shared_ptr<message> cmd_request_service::create_task_req_msg(bpo::variables_map& vm,
                                                                      ai::dbc::cmd_task_info & task_info)
        {
            std::string error;
            if (E_DEFAULT == validate_cmd_training_task_conf(vm,error))
            {
                task_info.result = "parameter error: " + error;
                return nullptr;
            }

            auto req_content = std::make_shared<matrix::service_core::start_training_req>();
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(AI_TRAINING_NOTIFICATION_REQ);
            req_content->header.__set_nonce(id_generator::generate_nonce());

            try
            {
                std::string task_id;
                if(vm.count("task_id"))
                {
                    task_id = vm["task_id"].as<std::string>();
                    std::string node_id = m_req_training_task_db.get_node_id_from_db(task_id);
                    if(node_id.empty())
                    {
                        LOG_ERROR << "not found node id of given task from db";
                        return nullptr;
                    }

                    std::vector<std::string> vec;
                    vec.push_back(node_id);
                    vm.at("peer_nodes_list").value() = vec;
                }

                if (task_id.empty())
                {
                    task_id = id_generator::generate_task_id();
                }

                req_content->body.__set_task_id(task_id);
                req_content->body.__set_select_mode(vm["select_mode"].as<int8_t>());
                req_content->body.__set_master(vm["master"].as<std::string>());
                req_content->body.__set_peer_nodes_list(vm["peer_nodes_list"].as<std::vector<std::string>>());
                req_content->body.__set_server_specification(vm["server_specification"].as<std::string>());
                req_content->body.__set_server_count(vm["server_count"].as<int32_t>());
                req_content->body.__set_training_engine(vm["training_engine"].as<std::string>());
                req_content->body.__set_code_dir(vm["code_dir"].as<std::string>());
                req_content->body.__set_entry_file(vm["entry_file"].as<std::string>());
                req_content->body.__set_data_dir(vm["data_dir"].as<std::string>());
                req_content->body.__set_checkpoint_dir(vm["checkpoint_dir"].as<std::string>());
                req_content->body.__set_hyper_parameters(vm["hyper_parameters"].as<std::string>());
                req_content->body.__set_container_name(vm["container_name"].as<std::string>());
                req_content->body.__set_memory(vm["memory"].as<int64_t>());
                req_content->body.__set_memory_swap(vm["memory_swap"].as<int64_t>());
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "keys missing" ;
                task_info.result = "parameters error: " + std::string(e.what());
                return nullptr;
            }

            // pack msg
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
            req_msg->set_content(req_content);

            task_info.task_id = req_content->body.task_id;

            // 创建签名
            std::map<std::string, std::string> exten_info;

            std::string message = req_content->body.task_id + req_content->body.code_dir + req_content->header.nonce;
            if (!use_sign_verify())
            {
                std::string sign = id_generator::sign(message, CONF_MANAGER->get_node_private_key());
                if (sign.empty())
                {
                    task_info.result = "sign error.pls check node key or task property";
                    return nullptr;
                }

                exten_info["sign"] = sign;
                exten_info["sign_algo"] = ECDSA;
                exten_info["sign_at"] = boost::str(boost::format("%d") %std::time(nullptr));
                exten_info["origin_id"] = CONF_MANAGER->get_node_id();
                req_content->header.__set_exten_info(exten_info);
            } else {
                if (E_SUCCESS != ai_crypto_util::extra_sign_info(message, exten_info))
                {
                    task_info.result = "sign error.pls check node key or task property";
                    return nullptr;
                }
            }

            exten_info["origin_id"] = CONF_MANAGER->get_node_id();
            for(auto& each : req_content->body.peer_nodes_list)
            {
                exten_info["dest_id"] += each + " ";
            }

            req_content->header.__set_exten_info(exten_info);

            return req_msg;
        }

        int32_t cmd_request_service::validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string& error)
        {
            std::string params[] = {
                    "training_engine",
                    "entry_file",
                    "code_dir"
            };

            for(const auto& param : params)
            {
                if (0 == vm.count(param))
                {
                    error = "missing param:" + param;
                    return E_DEFAULT;
                }
            }

            std::string engine_name = vm["training_engine"].as<std::string>();
            if (engine_name.empty() || engine_name.size() > MAX_ENGINE_IMGE_NAME_LEN)
            {
                error = "training_engine name length exceed maximum ";
                return E_DEFAULT;
            }

            std::string entry_file_name = vm["entry_file"].as<std::string>();
            if (entry_file_name.empty() || entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN)
            {
                error = "entry_file name length exceed maximum ";
                return E_DEFAULT;
            }

            std::string code_dir = vm["code_dir"].as<std::string>();
            if (code_dir.empty() || E_SUCCESS != validate_ipfs_path(code_dir))
            {
                error = "code_dir path is not valid";
                return E_DEFAULT;
            }

            if (0 != vm.count("data_dir") && !vm["data_dir"].as<std::string>().empty())
            {
                std::string data_dir = vm["data_dir"].as<std::string>();
                if (E_SUCCESS != validate_ipfs_path(data_dir)) {
                    error = "data_dir path is not valid";
                    return E_DEFAULT;
                }
            }

            if (0 == vm.count("peer_nodes_list") || vm["peer_nodes_list"].as<std::vector<std::string>>().empty())
            {
                if(code_dir != TASK_RESTART)
                {
                    error = "missing param:peer_nodes_list";
                    return E_DEFAULT;
                }
            }
            else
            {
                std::vector<std::string> nodes;
                nodes = vm["peer_nodes_list"].as<std::vector<std::string>>();
                for (auto &node_id : nodes)
                {
                    if (node_id.empty())
                        continue;

                    if (!id_generator::check_base58_id(node_id))
                    {
                        error = "node value does not match the Base58 code format";
                        return E_DEFAULT;
                    }
                }
            }

            return E_SUCCESS;
        }

        int32_t cmd_request_service::validate_ipfs_path(const std::string &path_arg)
        {
            if(path_arg == std::string(NODE_REBOOT) || path_arg == std::string(TASK_RESTART))
            {
                return E_SUCCESS;
            }

            cregex reg = cregex::compile("(^[a-zA-Z0-9]{46}$)");
            if (regex_match(path_arg.c_str(), reg))
            {
                return E_SUCCESS;
            }
            return E_DEFAULT;
        }

        int32_t cmd_request_service::validate_entry_file_name(const std::string &entry_file_name)
        {
            /*cregex reg = cregex::compile("(^[0-9a-zA-Z]{1,}[-_]{0,}[0-9a-zA-Z]{0,}.py$)");
            if (regex_match(entry_file_name.c_str(), reg))
            {
                return E_SUCCESS;
            }*/
            //return E_DEFAULT;
            if (entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN)
            {
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_cmd_stop_training_req(const std::shared_ptr<message> &msg)
        {
            std::shared_ptr<::cmd_stop_training_req> cmd_req_content = std::dynamic_pointer_cast<::cmd_stop_training_req>(msg->get_content());
            if (cmd_req_content == nullptr)
            {
                LOG_ERROR << "cmd_req_content is null";
                return E_NULL_POINTER;
            }

            std::shared_ptr<::cmd_stop_training_resp> cmd_resp = std::make_shared<::cmd_stop_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";

            if (nullptr != cmd_req_content) {
                cmd_resp->header.__set_session_id(cmd_req_content->header.session_id);
            }

            const std::string &task_id = cmd_req_content->task_id;

            ai::dbc::cmd_task_info task_info_in_db;
            if(!m_req_training_task_db.read_task_info_from_db(task_id, task_info_in_db))
            {
                LOG_ERROR << "ai power requester service cmd stop task, task id invalid: " << task_id;
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task id is invalid";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();

            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(STOP_TRAINING_REQ);
            req_content->header.__set_nonce(id_generator::generate_nonce());

            req_content->body.__set_task_id(task_id);

            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);

            std::map<std::string, std::string> exten_info;
            std::string sign_msg = req_content->body.task_id + req_content->header.nonce;
            if (!use_sign_verify())
            {
                std::string sign = id_generator::sign(sign_msg, CONF_MANAGER->get_node_private_key());
                if (sign.empty())
                {
                    cmd_resp->result = E_DEFAULT;
                    cmd_resp->result_info = "sign error. pls check node key or task property";
                    TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_training_resp).name(), cmd_resp);
                    return E_DEFAULT;
                }

                exten_info["sign"] = sign;
                exten_info["sign_algo"] = ECDSA;
                exten_info["sign_at"] = boost::str(boost::format("%d") %std::time(nullptr));
            } else {
                if (E_SUCCESS != ai_crypto_util::extra_sign_info(sign_msg, exten_info))
                {
                    return E_DEFAULT;
                }
            }

            exten_info["origin_id"] = CONF_MANAGER->get_node_id();

            if(!task_info_in_db.peer_nodes_list.empty()) {
                exten_info["dest_id"] = task_info_in_db.peer_nodes_list[0];
            }

            req_content->header.__set_exten_info(exten_info);

            if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(req_msg))
            {
                cmd_resp->result = E_INACTIVE_CHANNEL;
                cmd_resp->result_info = "dbc node don't connect to network, pls check ";
            }

            TOPIC_MANAGER->publish<void>(typeid(::cmd_stop_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_cmd_list_training_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_resp = std::make_shared<::cmd_list_training_resp>();
            auto cmd_req = std::dynamic_pointer_cast<::cmd_list_training_req>(msg->get_content());
            if (nullptr != cmd_req) {
                cmd_resp->header.__set_session_id(cmd_req->header.session_id);
            }

            // 0: list all tasks;
            // 1: list specific tasks
            std::vector<ai::dbc::cmd_task_info> vec_task_infos;
            if (1 == cmd_req->list_type)
            {
                if (!m_req_training_task_db.read_task_info_from_db(cmd_req->task_list, vec_task_infos))
                {
                    LOG_ERROR << "load task info failed.";
                }
            }
            else
            {
                if (!m_req_training_task_db.read_task_info_from_db(vec_task_infos))
                {
                    LOG_ERROR << "failed to load all task info from db.";
                }
            }

            if (vec_task_infos.empty())
            {
                cmd_resp->result = E_DEFAULT;

                if (1 == cmd_req->list_type)
                {
                    cmd_resp->result_info = "specified task not exist";
                }
                else
                {
                    cmd_resp->result_info = "task list is empty";
                }

                TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            std::sort(vec_task_infos.begin(), vec_task_infos.end()
                    , [](ai::dbc::cmd_task_info task1, ai::dbc::cmd_task_info task2) -> bool {
                return task1.create_time > task2.create_time;
            });

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::list_training_req>();

            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(LIST_TRAINING_REQ);
            req_content->header.__set_nonce(id_generator::generate_nonce());
            if (nullptr != cmd_req) {
                req_content->header.__set_session_id(cmd_req->header.session_id);
            }

            std::vector<std::string> path;
            path.push_back(CONF_MANAGER->get_node_id());
            req_content->header.__set_path(path);

            auto vec_task_infos_to_show = std::make_shared<std::vector<ai::dbc::cmd_task_info> >();
            for (const auto& info : vec_task_infos)
            {
                if (info.status < task_stopped)
                {
                    if (vec_task_infos_to_show->size() < MAX_TASK_SHOWN_ON_LIST)
                    {
                        req_content->body.task_list.push_back(info.task_id);
                        vec_task_infos_to_show->push_back(info);
                    } else {
                        break;
                    }
                }
            }

            if (1 == cmd_req->list_type)
            {
                *vec_task_infos_to_show = vec_task_infos;
            }
            else
            {
                for (const auto& info : vec_task_infos)
                {
                    if (vec_task_infos_to_show->size() >= MAX_TASK_SHOWN_ON_LIST)
                        break;

                    if (info.status & (task_stopped | task_successfully_closed | task_abnormally_closed | task_overdue_closed))
                    {
                        vec_task_infos_to_show->push_back(info);
                    }
                }
            }

            //check if no need network req
            if (req_content->body.task_list.empty())
            {
                cmd_resp->result = E_SUCCESS;
                for (const auto& info : *vec_task_infos_to_show)
                {
                    ::cmd_task_status cts;
                    cts.task_id = info.task_id;
                    cts.status = info.status;
                    cts.create_time = info.create_time;
                    cts.description = info.description;
                    cts.raw = info.raw;
                    cmd_resp->task_status_list.push_back(std::move(cts));
                }

                TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            if (!CONNECTION_MANAGER->have_active_channel())
            {
                // early false response to avoid timer/session operations
                cmd_resp->result = E_INACTIVE_CHANNEL;
                cmd_resp->result_info = "dbc node do not connect to network, pls check.";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);

            //add timer
            uint32_t timer_id = add_timer(LIST_TRAINING_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(), ONLY_ONE_TIME, req_content->header.session_id);

            //service session
            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);

            //session context
            variable_value val;
            val.value() = req_msg;
            session->get_context().add("req_msg", val);
            variable_value show_tasks;
            show_tasks.value() = vec_task_infos_to_show;
            session->get_context().add("show_tasks", show_tasks);
            variable_value task_ids;
            task_ids.value() = std::make_shared<std::unordered_map<std::string, int8_t>>();
            session->get_context().add("task_ids", task_ids);

            std::string message = boost::algorithm::join(req_content->body.task_list, "") + req_content->header.nonce + req_content->header.session_id;
            std::map<std::string, std::string> exten_info;

            exten_info["origin_id"] = CONF_MANAGER->get_node_id();

            if (E_SUCCESS != ai_crypto_util::extra_sign_info(message,exten_info))
            {
                return E_DEFAULT;
            }
            req_content->header.__set_exten_info(exten_info);

            //add session
            int32_t ret = this->add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                remove_timer(timer_id);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error while processing this cmd";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(::cmd_list_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_cmd_logs_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_req = std::dynamic_pointer_cast<::cmd_logs_req>(msg->get_content());
            if (nullptr == cmd_req)
            {
                LOG_ERROR << "ai power requester service cmd logs msg content nullptr";
                return E_DEFAULT;
            }

            std::shared_ptr<::cmd_logs_resp> cmd_resp = std::make_shared<::cmd_logs_resp>();
            if (nullptr != cmd_req) {
                cmd_resp->header.__set_session_id(cmd_req->header.session_id);
            }

            ai::dbc::cmd_task_info task_info_in_db;
            if(!m_req_training_task_db.read_task_info_from_db(cmd_req->task_id, task_info_in_db))
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task id not found error";

                TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            if (GET_LOG_HEAD != cmd_req->head_or_tail && GET_LOG_TAIL != cmd_req->head_or_tail)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "log direction error";

                TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            if (cmd_req->number_of_lines > MAX_NUMBER_OF_LINES)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "number of lines error: should less than " + std::to_string(cmd_req->number_of_lines);

                TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::logs_req>();

            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(LOGS_REQ);
            req_content->header.__set_nonce(id_generator::generate_nonce());
            if (nullptr != cmd_req) {
                req_content->header.__set_session_id(cmd_req->header.session_id);
            }

            std::vector<std::string> path;
            path.push_back(CONF_MANAGER->get_node_id());
            req_content->header.__set_path(path);

            req_content->body.__set_task_id(cmd_req->task_id);
            req_content->body.__set_head_or_tail(cmd_req->head_or_tail);
            req_content->body.__set_number_of_lines(cmd_req->number_of_lines);

            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);

            uint32_t timer_id = add_timer(TASK_LOGS_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(), ONLY_ONE_TIME, req_content->header.session_id);

            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);

            variable_value val;
            val.value() = req_msg;
            session->get_context().add("req_msg", val);

            if (cmd_req->sub_op == "result")
            {
                variable_value v1;
                v1.value() = std::string(cmd_req->sub_op);
                session->get_context().add("sub_op", v1);

                variable_value v2;
                v2.value() = cmd_req->dest_folder;
                session->get_context().add("dest_folder", v2);
            } else if(cmd_req->sub_op == "log") {
                variable_value v1;
                v1.value() = std::string(cmd_req->sub_op);
                session->get_context().add("sub_op", v1);

                variable_value v2;
                v2.value() = cmd_req->task_id;
                session->get_context().add("task_id", v2);
            }

            if (!CONNECTION_MANAGER->have_active_channel())
            {
                cmd_resp->result = E_INACTIVE_CHANNEL;
                cmd_resp->result_info = "dbc node do not connect to network, pls check.";
                TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            std::map<std::string, std::string> exten_info;
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();
            if(!task_info_in_db.peer_nodes_list.empty())
            {
                exten_info["dest_id"] = task_info_in_db.peer_nodes_list[0];
            }

            std::string message = req_content->body.task_id + req_content->header.nonce + req_content->header.session_id;
            if (E_SUCCESS != ai_crypto_util::extra_sign_info(message,exten_info))
            {
                return E_DEFAULT;
            }

            req_content->header.__set_exten_info(exten_info);

            int32_t ret = this->add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                remove_timer(timer_id);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error while processing this cmd";

                TOPIC_MANAGER->publish<void>(typeid(::cmd_logs_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }

        int32_t cmd_request_service::on_cmd_task_clean(const std::shared_ptr<message> &msg)
        {
            auto cmd_req_content = std::dynamic_pointer_cast<::cmd_task_clean_req>(msg->get_content());
            if (!cmd_req_content)
            {
                LOG_ERROR << "cmd_req_content is null";
                return E_NULL_POINTER;
            }

            std::vector<ai::dbc::cmd_task_info> vec_task_infos;
            if (!m_req_training_task_db.read_task_info_from_db(vec_task_infos))
            {
                LOG_ERROR << "failed to load all task info from db.";
            }

            auto cmd_resp = std::make_shared<::cmd_task_clean_resp>();

            if (nullptr != cmd_req_content) {
                cmd_resp->header.__set_session_id(cmd_req_content->header.session_id);
            }

            if (vec_task_infos.empty())
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task list is empty";

                TOPIC_MANAGER->publish<void>(typeid(::cmd_task_clean_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            cmd_resp->result_info = "\n";
            for(auto& task: vec_task_infos)
            {
                try
                {
                    bool cleanable = (cmd_req_content->clean_all)
                                     || (cmd_req_content->task_id.empty() && task.status == task_unknown)
                                     || (cmd_req_content->task_id == task.task_id &&
                                         (task.status != task_running && task.status != task_pulling_image &&
                                          task.status != task_queueing));

                    if (cleanable)
                    {
                        m_req_training_task_db.delete_task(task.task_id);
                        cmd_resp->result_info += "\t" + task.task_id + "\n";
                    }
                }
                catch(...)
                {
                    LOG_ERROR << "failed to delete task info from db; task_id:= " << task.task_id;
                }
            }

            cmd_resp->result = E_SUCCESS;
            TOPIC_MANAGER->publish<void>(typeid(::cmd_task_clean_resp).name(), cmd_resp);

            return E_SUCCESS;
        }
    }
}
