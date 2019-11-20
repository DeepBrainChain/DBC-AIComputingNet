/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name      :   ai_cmd_line.cpp
* description    :   cmd line callback and their helper. Move from ai_power_provider_service.cpp.
* date           :   2019.05.27
* author         :
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
        void ai_power_requestor_service::init_subscription_part2()
        {
            //cmd start training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_start_training_req).name());


            //cmd stop training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_stop_training_req).name());

            //cmd list training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_list_training_req).name());

            //cmd logs req
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_logs_req).name());

            //cmd clean task req
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_task_clean_req).name());
        }


        void ai_power_requestor_service::init_invoker_part2()
        {
            invoker_type invoker;

            BIND_MESSAGE_INVOKER(typeid(cmd_start_training_req).name(), &ai_power_requestor_service::on_cmd_start_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_stop_training_req).name(), &ai_power_requestor_service::on_cmd_stop_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_list_training_req).name(), &ai_power_requestor_service::on_cmd_list_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_logs_req).name(), &ai_power_requestor_service::on_cmd_logs_req);

            BIND_MESSAGE_INVOKER(typeid(cmd_task_clean_req).name(), &ai_power_requestor_service::on_cmd_task_clean);

//            BIND_MESSAGE_INVOKER(LIST_TRAINING_RESP, &ai_power_requestor_service::on_list_training_resp);
//            BIND_MESSAGE_INVOKER(LOGS_RESP, &ai_power_requestor_service::on_logs_resp);
//
//            BIND_MESSAGE_INVOKER(BINARY_FORWARD_MSG, &ai_power_requestor_service::on_binary_forward);

        }


        int32_t ai_power_requestor_service::on_cmd_start_training_req(std::shared_ptr<message> &msg)
        {
            bpo::variables_map vm;

            std::shared_ptr<ai::dbc::cmd_start_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_info.task_id = "";
            cmd_resp->task_info.create_time = time(nullptr);
            cmd_resp->task_info.status = task_unknown;

            std::shared_ptr<base> content = msg->get_content();
            LOG_INFO << "on_cmd_start_training_req content: " << content;
            std::shared_ptr<cmd_start_training_req> req = std::dynamic_pointer_cast<cmd_start_training_req>(content);
            assert(nullptr != req && nullptr != content);
            COPY_MSG_HEADER(req,cmd_resp);

            if (!req || !content)
            {
                LOG_ERROR << "null ptr of req";
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }


            auto req_msg = create_task_msg(req->vm, cmd_resp->task_info);
            if (nullptr == req_msg)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = cmd_resp->task_info.result;
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            LOG_DEBUG << "ai power requester service broadcast start training msg, nonce: " << req_msg->get_content()->header.nonce;
            LOG_INFO << "ai power requester service broadcast start training msg, nonce req->vm: " << req->vm;

            if (CONNECTION_MANAGER->broadcast_message(req_msg) != E_SUCCESS)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "submit error. pls check network";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //peer won't reply, so public resp directly
            cmd_resp->result = E_SUCCESS;

            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

            //flush to db
            std::string code_dir=req->vm["code_dir"].as<std::string>();
            LOG_DEBUG << "code_dir " << code_dir;
            if ( code_dir == std::string(NODE_REBOOT) )
            {
                LOG_DEBUG << "not serialize for reboot task";
            }
            else
            {

                if ( code_dir == std::string(TASK_RESTART) )
                {
                    // keep original task description

                    // reset task status as task_unknown
                    m_req_training_task_db.reset_task_status_to_db(cmd_resp->task_info.task_id);

                }
                else
                {

                    std::string task_description;
                    try
                    {
                        std::vector<std::string> nodes = req->vm["peer_nodes_list"].as<std::vector<std::string>>();
                        std::string node = nodes[0];
                        task_description = node;
                        //                    task_description=node.substr(node.length()-8);

                        cmd_resp->task_info.peer_nodes_list.push_back(node);
                    }
                    catch (const std::exception &e)
                    {
                        LOG_ERROR << std::string(e.what());
                    }


                    task_description += " : " + req->parameters["description"];
                    cmd_resp->task_info.__set_description(task_description);



                    std::ostringstream stream;
                    auto msg_content = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(req_msg->content);
                    if (msg_content != nullptr)
                    {
                        std::ostringstream stream;
                        stream << msg_content->body;
                        LOG_INFO << "body: " << msg_content->body;
                        LOG_INFO << "raw: " << stream.str();
                        cmd_resp->task_info.__set_raw(stream.str());
                    }

                    m_req_training_task_db.write_task_info_to_db(cmd_resp->task_info);
                }

            }

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string& error)
        {


            std::string s[]={
                "training_engine",
                "entry_file",
                "code_dir"
            };

            for(auto& i : s)
            {
                if ( 0 == vm.count(i) || vm[i].as<std::string>().empty())
                {
                    error = i + " absent";
                    return E_DEFAULT;
                }
            }

            std::string engine_name = vm["training_engine"].as<std::string>();
            if (check_task_engine(engine_name) != true)
            {
                error = "training_engine format is not correct ";
                return E_DEFAULT;
            }

            if (engine_name.size() > MAX_ENGINE_IMGE_NAME_LEN)
            {
                error = "training_engine name length exceed maximum ";
                return E_DEFAULT;
            }


            if (E_SUCCESS != validate_entry_file_name(vm["entry_file"].as<std::string>()))
            {
                error = "entry_file name length exceed maximum ";

                return E_DEFAULT;
            }


            if (E_SUCCESS != validate_ipfs_path(vm["code_dir"].as<std::string>()))
            {
                error = "code_dir path is not valid";
                return E_DEFAULT;
            }

            if (0 != vm.count("data_dir") && !vm["data_dir"].as<std::string>().empty() && E_SUCCESS != validate_ipfs_path(vm["data_dir"].as<std::string>()))
            {
                error = "data_dir path is not valid";
                return E_DEFAULT;
            }


            if (0 == vm.count("peer_nodes_list") || vm["peer_nodes_list"].as<std::vector<std::string>>().empty())
            {
                if(vm["code_dir"].as<std::string>() != std::string(TASK_RESTART))
                {
                    error = "peer_nodes_list absent";
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
                    {
                        error = "empty node value";
                        return E_DEFAULT;
                    }
                    else if (false == id_generator().check_base58_id(node_id))
                    {
                        error = "node value does not match the Base58 code format";
                        return E_DEFAULT;
                    }
                }
            }

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::validate_ipfs_path(const std::string &path_arg)
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

        int32_t ai_power_requestor_service::validate_entry_file_name(const std::string &entry_file_name)
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


        int32_t ai_power_requestor_service::on_cmd_stop_training_req(const std::shared_ptr<message> &msg)
        {
            std::shared_ptr<cmd_stop_training_req> cmd_req_content = std::dynamic_pointer_cast<cmd_stop_training_req>(msg->get_content());
            assert(nullptr != cmd_req_content);
            if (!cmd_req_content)
            {
                LOG_ERROR << "cmd_req_content is null";
                return E_NULL_POINTER;
            }

            std::shared_ptr<cmd_stop_training_resp> cmd_resp = std::make_shared<cmd_stop_training_resp>();
            COPY_MSG_HEADER(cmd_req_content,cmd_resp);


            const std::string &task_id = cmd_req_content->task_id;

            //check valid
//            if (!is_task_exist_in_db(task_id))

            ai::dbc::cmd_task_info task_info_in_db;
            if(!m_req_training_task_db.read_task_info_from_db(task_id, task_info_in_db))
            {
                LOG_ERROR << "ai power requester service cmd stop task, task id invalid: " << task_id;
                //public resp directly
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task id is invalid";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();

            //header
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(STOP_TRAINING_REQ);
            req_content->header.__set_nonce(id_generator().generate_nonce());

            //body
            req_content->body.__set_task_id(task_id);

            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);


            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";


            std::map<std::string, std::string> exten_info;

            std::string sign_msg = req_content->body.task_id + req_content->header.nonce;
            // change on sign message:
            //     timestamp is added to the signature input.
            if (!use_sign_verify())
            {
                std::string sign = id_generator().sign(sign_msg, CONF_MANAGER->get_node_private_key());
                if (sign.empty())
                {
                    cmd_resp->result = E_DEFAULT;
                    cmd_resp->result_info = "sign error. pls check node key or task property";
                    TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);
                    return E_DEFAULT;
                }
//                std::map<std::string, std::string> exten_info;
                exten_info["sign"] = sign;
                exten_info["sign_algo"] = ECDSA;
                exten_info["sign_at"] = boost::str(boost::format("%d") %std::time(nullptr));
//                req_content->header.__set_exten_info(exten_info);
            }
            else
            {
                if (E_SUCCESS != ai_crypto_util::extra_sign_info(sign_msg, exten_info))
                {
                    return E_DEFAULT;
                }
            }

            // needs to fill dest_id with the "peer_list" from task db
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();

            if(task_info_in_db.peer_nodes_list.size() > 0)
            {
                exten_info["dest_id"] = task_info_in_db.peer_nodes_list[0];
            }

            req_content->header.__set_exten_info(exten_info);


            if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(req_msg))
            {
                cmd_resp->result = E_INACTIVE_CHANNEL;
                cmd_resp->result_info = "dbc node don't connect to network, pls check ";
            }

            //there's no reply, so public resp directly
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_list_training_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();

            auto cmd_req = std::dynamic_pointer_cast<cmd_list_training_req>(msg->get_content());

            COPY_MSG_HEADER(cmd_req,cmd_resp);


            std::vector<ai::dbc::cmd_task_info> vec_task_infos;
            if (1 == cmd_req->list_type) //0: list all tasks; 1: list specific tasks
            {
                //check task id exists
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

            //task list is empty
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

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            //cache task infos to show on cmd console
            auto vec_task_infos_to_show = std::make_shared<std::vector<ai::dbc::cmd_task_info> >();

            //sort by create_time
            std::sort(vec_task_infos.begin(), vec_task_infos.end()
                , [](ai::dbc::cmd_task_info task1, ai::dbc::cmd_task_info task2) -> bool { return task1.create_time > task2.create_time; });

            //prepare for resp
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::list_training_req>();

            //header
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(LIST_TRAINING_REQ);
            req_content->header.__set_nonce(id_generator().generate_nonce());
            COPY_MSG_HEADER(cmd_req,req_content);



            //for efficient resp msg transport
            std::vector<std::string> path;
            path.push_back(CONF_MANAGER->get_node_id());
            req_content->header.__set_path(path);

            //body
            for (auto info : vec_task_infos)
            {
                //add unclosed task to request
                //if (info.status & (task_unknown | task_queueing | task_running))
                if (info.status < task_stopped)
                {
                    //case: more than MAX_TASK_SHOWN_ON_LIST
                    if (vec_task_infos_to_show->size() < MAX_TASK_SHOWN_ON_LIST)
                    {
                        req_content->body.task_list.push_back(info.task_id);
                        vec_task_infos_to_show->push_back(info);
                    }
                }
            }

            //prepare infos to show on cmd console
            if (0 == cmd_req->list_type)
            {
                for (auto info : vec_task_infos)
                {
                    if (vec_task_infos_to_show->size() >= MAX_TASK_SHOWN_ON_LIST)
                        break;
                    //append some closed task to show on cmd console
                    if (info.status & (task_stopped | task_successfully_closed | task_abnormally_closed | task_overdue_closed))
                    {
                        vec_task_infos_to_show->push_back(info);
                    }
                }
            }
            else if (1 == cmd_req->list_type)
            {
                *vec_task_infos_to_show = vec_task_infos;//always has one element
            }
            //check if no need network req
            if (req_content->body.task_list.empty())
            {
                //return cmd resp
                cmd_resp->result = E_SUCCESS;
                for (auto info : *vec_task_infos_to_show)
                {
                    cmd_task_status cts;
                    cts.task_id = info.task_id;
                    cts.status = info.status;
                    cts.create_time = info.create_time;
                    cts.description = info.description;
                    cts.raw = info.raw;
                    cmd_resp->task_status_list.push_back(std::move(cts));
                }

                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            if (!CONNECTION_MANAGER->have_active_channel())
            {
                // early false response to avoid timer/session operations
                cmd_resp->result = E_INACTIVE_CHANNEL;
                cmd_resp->result_info = "dbc node do not connect to network, pls check.";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }


            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);

            //add to timer
            uint32_t timer_id = add_timer(LIST_TRAINING_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(), ONLY_ONE_TIME, req_content->header.session_id);
            assert(INVALID_TIMER_ID != timer_id);

            //service session
            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);

            //session context
            variable_value val;
            val.value() = req_msg;
            session->get_context().add("req_msg", val);
            //cache task infos to show on cmd console
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

            //add to session
            int32_t ret = this->add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power requester service list training add session error: " << session->get_session_id();

                //remove timer
                remove_timer(timer_id);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error while processing this cmd";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            LOG_DEBUG << "ai power requester service list training add session: " << session->get_session_id();

            //ok, broadcast
            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }


        std::shared_ptr<message> ai_power_requestor_service::create_task_msg(bpo::variables_map& vm,
                                                                             ai::dbc::cmd_task_info & task_info)
        {

            //validate parameters
            std::string error;
            if (E_DEFAULT == validate_cmd_training_task_conf(vm,error))
            {
                task_info.result = "parameter error: " + error;
                return nullptr;
            }


            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::start_training_req> req_content = std::make_shared<matrix::service_core::start_training_req>();

            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(AI_TRAINING_NOTIFICATION_REQ);
            req_content->header.__set_nonce(id_generator().generate_nonce());

            try
            {

                std::string task_id;
                if(vm.count("task_id"))
                {
                    // in case task restart, it will specify the task id in cmd line
                    task_id = vm["task_id"].as<std::string>();

                    std::string node_id = m_req_training_task_db.get_node_id_from_db(task_id);

                    if(node_id.empty())
                    {
                        LOG_ERROR << "not found node id of given task from db";
                        return nullptr;
                    }

                    std::vector<std::string> v;
                    v.push_back(node_id);
                    vm.at("peer_nodes_list").value() = v;
                }

                if (task_id.empty())
                {
                    task_id = id_generator().generate_task_id();
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
                LOG_INFO << "vm[\"container_name\"].as<int64_t>():"+vm["container_name"].as<std::string>();
                LOG_INFO << "vm[\"memory\"].as<int64_t>():"+vm["memory"].as<int64_t>();
                req_content->body.__set_memory_swap(vm["memory_swap"].as<int64_t>());
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "keys missing" ;
                task_info.result = "parameters error: " + std::string(e.what());
                return nullptr;
            }

            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
            req_msg->set_content(req_content);
            task_info.task_id = req_content->body.task_id;

            std::map<std::string, std::string> exten_info;

            std::string message = req_content->body.task_id + req_content->body.code_dir + req_content->header.nonce;
            if (!use_sign_verify())
            {
                std::string sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());
                if (sign.empty())
                {
                    task_info.result = "sign error.pls check node key or task property";
                    return nullptr;
                }

//                std::map<std::string, std::string> exten_info;
                exten_info["sign"] = sign;
                exten_info["sign_algo"] = ECDSA;
                exten_info["sign_at"] = boost::str(boost::format("%d") %std::time(nullptr));
                exten_info["origin_id"] = CONF_MANAGER->get_node_id();
                req_content->header.__set_exten_info(exten_info);
            }
            else
            {

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



        int32_t ai_power_requestor_service::on_cmd_logs_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_req = std::dynamic_pointer_cast<cmd_logs_req>(msg->get_content());
            if (nullptr == cmd_req)
            {
                LOG_ERROR << "ai power requester service cmd logs msg content nullptr";
                return E_DEFAULT;
            }

            //cmd resp
            std::shared_ptr<ai::dbc::cmd_logs_resp> cmd_resp = std::make_shared<ai::dbc::cmd_logs_resp>();
            COPY_MSG_HEADER(cmd_req,cmd_resp);


            //check task id
//            std::string task_value;
//            leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), cmd_req->task_id, &task_value);
//            if (!status.ok())

            ai::dbc::cmd_task_info task_info_in_db;
            if(!m_req_training_task_db.read_task_info_from_db(cmd_req->task_id, task_info_in_db))
            {
                LOG_ERROR << "ai power requester service cmd logs check task id error: " << cmd_req->task_id;

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task id not found error";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }


            //check head or tail
            if (GET_LOG_HEAD != cmd_req->head_or_tail && GET_LOG_TAIL != cmd_req->head_or_tail)
            {
                LOG_ERROR << "ai power requester service cmd logs check log direction error: " << std::to_string(cmd_req->head_or_tail);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "log direction error";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            //check number of lines
            if (cmd_req->number_of_lines > MAX_NUMBER_OF_LINES)
            {
                LOG_ERROR << "ai power requester service cmd logs check number of lines error: " << std::to_string(cmd_req->number_of_lines);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "number of lines error: should less than " + std::to_string(cmd_req->number_of_lines);

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::logs_req>();

            //header
            req_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            req_content->header.__set_msg_name(LOGS_REQ);
            req_content->header.__set_nonce(id_generator().generate_nonce());
            COPY_MSG_HEADER(cmd_req,req_content);

            //for efficient resp msg transport
            std::vector<std::string> path;
            path.push_back(CONF_MANAGER->get_node_id());
            req_content->header.__set_path(path);

            req_content->body.__set_task_id(cmd_req->task_id);
            //req_content->body.peer_nodes_list
            req_content->body.__set_head_or_tail(cmd_req->head_or_tail);
            req_content->body.__set_number_of_lines(cmd_req->number_of_lines);

            req_msg->set_content(req_content);
            req_msg->set_name(req_content->header.msg_name);

            //add to timer
            uint32_t timer_id = add_timer(TASK_LOGS_TIMER, CONF_MANAGER->get_timer_dbc_request_in_millisecond(), ONLY_ONE_TIME, req_content->header.session_id);
            assert(INVALID_TIMER_ID != timer_id);

            //service session
            std::shared_ptr<service_session> session = std::make_shared<service_session>(timer_id, req_content->header.session_id);

            //session context
            variable_value val;
            val.value() = req_msg;
            session->get_context().add("req_msg", val);


            // sub operation support: download training result or display training log
            if (cmd_req->sub_op == "result")
            {
                variable_value v1;
                v1.value() = std::string(cmd_req->sub_op);
                session->get_context().add("sub_op", v1);

                variable_value v2;
                v2.value() = cmd_req->dest_folder;
                session->get_context().add("dest_folder", v2);
            }
            else if(cmd_req->sub_op == "log")
            {
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
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);
                return E_DEFAULT;
            }


            std::map<std::string, std::string> exten_info;
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();
            if(task_info_in_db.peer_nodes_list.size() > 0)
            {
                exten_info["dest_id"] = task_info_in_db.peer_nodes_list[0];
            }

            std::string message = req_content->body.task_id + req_content->header.nonce + req_content->header.session_id;
            if (E_SUCCESS != ai_crypto_util::extra_sign_info(message,exten_info))
            {
                return E_DEFAULT;
            }

            req_content->header.__set_exten_info(exten_info);

            //add to session
            int32_t ret = this->add_session(session->get_session_id(), session);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "ai power requester service logs add session error: " << session->get_session_id();

                //remove timer
                remove_timer(timer_id);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "internal error while processing this cmd";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_logs_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            LOG_DEBUG << "ai power requester service logs add session: " << session->get_session_id();

            //ok, broadcast
            CONNECTION_MANAGER->broadcast_message(req_msg);

            return E_SUCCESS;
        }

        
        int32_t ai_power_requestor_service::on_cmd_task_clean(const std::shared_ptr<message> &msg)
        {
            auto cmd_req_content = std::dynamic_pointer_cast<cmd_task_clean_req>(msg->get_content());
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

            auto cmd_resp = std::make_shared<ai::dbc::cmd_task_clean_resp>();

            COPY_MSG_HEADER(cmd_req_content,cmd_resp);

            //task list is empty
            if (vec_task_infos.empty())
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "task list is empty";

                //return cmd resp
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_task_clean_resp).name(), cmd_resp);
                return E_SUCCESS;
            }

            cmd_resp->result_info = "\n";
            for(auto& task: vec_task_infos)
            {
                try
                {
                    bool cleanable = (cmd_req_content->clean_all)
                         || (cmd_req_content->task_id.empty() && task.status == task_unknown)
                         || (cmd_req_content->task_id == task.task_id && (task.status != task_running && task.status != task_pulling_image && task.status != task_queueing));

                    if (cleanable)
                    {
                        LOG_INFO << "delete task " << task.task_id;
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
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_task_clean_resp).name(), cmd_resp);

            return E_SUCCESS;
        }
    }//service_core
}
