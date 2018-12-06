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

            ret = init_db();

            return ret;
        }


        void ai_power_requestor_service::init_subscription()
        {
            //cmd start training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_start_training_req).name());

            //cmd start multi training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_start_multi_training_req).name());

            //cmd stop training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_stop_training_req).name());

            //cmd list training
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_list_training_req).name());

            //cmd logs req
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_logs_req).name());

            //cmd logs req
            //SUBSCRIBE_BUS_MESSAGE(typeid(cmd_clear_req).name());

            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_ps_req).name());
            SUBSCRIBE_BUS_MESSAGE(typeid(cmd_task_clean_req).name());
            //list training resp
            SUBSCRIBE_BUS_MESSAGE(LIST_TRAINING_RESP);

            //logs resp
            SUBSCRIBE_BUS_MESSAGE(LOGS_RESP);
        }

        void ai_power_requestor_service::init_invoker()
        {
            invoker_type invoker;

            BIND_MESSAGE_INVOKER(typeid(cmd_start_training_req).name(), &ai_power_requestor_service::on_cmd_start_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_start_multi_training_req).name(), &ai_power_requestor_service::on_cmd_start_multi_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_stop_training_req).name(), &ai_power_requestor_service::on_cmd_stop_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_list_training_req).name(), &ai_power_requestor_service::on_cmd_list_training_req);
            BIND_MESSAGE_INVOKER(typeid(cmd_logs_req).name(), &ai_power_requestor_service::on_cmd_logs_req);

            //BIND_MESSAGE_INVOKER(typeid(cmd_clear_req).name(), &ai_power_requestor_service::on_cmd_clear);
            BIND_MESSAGE_INVOKER(typeid(cmd_ps_req).name(), &ai_power_requestor_service::on_cmd_ps);
            BIND_MESSAGE_INVOKER(typeid(cmd_task_clean_req).name(), &ai_power_requestor_service::on_cmd_task_clean);

            BIND_MESSAGE_INVOKER(LIST_TRAINING_RESP, &ai_power_requestor_service::on_list_training_resp);
            BIND_MESSAGE_INVOKER(LOGS_RESP, &ai_power_requestor_service::on_logs_resp);

        }

        int32_t ai_power_requestor_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options options;
            options.create_if_missing = true;

            //get db path
            fs::path task_db_path = env_manager::get_db_path();
            try
            {
                if (false == fs::exists(task_db_path))
                {
                    LOG_DEBUG << "db directory path does not exist and create db directory";
                    fs::create_directory(task_db_path);
                }

                //check db directory
                if (false == fs::is_directory(task_db_path))
                {
                    LOG_ERROR << "db directory path does not exist and exit";
                    return E_DEFAULT;
                }

                task_db_path /= fs::path("req_training_task.db");
                LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

                //open db
                leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);

                if (false == status.ok())
                {
                    LOG_ERROR << "ai power requestor service init training task db error: " << status.ToString();
                    return E_DEFAULT;
                }

                //smart point auto close db
                m_req_training_task_db.reset(db);
                LOG_INFO << "ai power requestor training db path:" << task_db_path;
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create task req db error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create task req db error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        void ai_power_requestor_service::init_timer()
        {
            m_timer_invokers[LIST_TRAINING_TIMER] = std::bind(&ai_power_requestor_service::on_list_training_timer, this, std::placeholders::_1);
            m_timer_invokers[TASK_LOGS_TIMER] = std::bind(&ai_power_requestor_service::on_logs_timer, this, std::placeholders::_1);
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
            write_task_info_to_db(cmd_resp->task_info);

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
                error = "peer_nodes_list absent";
                return E_DEFAULT;
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

        int32_t ai_power_requestor_service::on_cmd_start_multi_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<cmd_start_multi_training_req> cmd_req_content = std::dynamic_pointer_cast<cmd_start_multi_training_req>(content);

            //cmd resp
            std::shared_ptr<ai::dbc::cmd_start_multi_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_multi_training_resp>();
            COPY_MSG_HEADER(cmd_req_content,cmd_resp);
            if (!cmd_req_content)
            {
                LOG_ERROR << "ai power requester service on cmd start multi training received null msg";

                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "start multi training error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }



            bpo::variables_map multi_vm;

            try
            {
                //parse multi task config file
                std::ifstream multi_task_conf(cmd_req_content->mulit_task_file_path);
                bpo::store(bpo::parse_config_file(multi_task_conf, cmd_req_content->multi_tasks_config_opts), multi_vm);
                bpo::notify(multi_vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "multi task config file parse error: " << diagnostic_information(e);

                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "multi task config file parse error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //parse task config empty
            if (0 == multi_vm.count("training_file") || 0 == multi_vm["training_file"].as<std::vector<std::string>>().size())
            {
                //error resp
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "multi task config file parse error, maybe there's no training file";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //parse each task config
            bpo::variables_map vm;

            const std::vector<std::string> & files = multi_vm["training_file"].as<std::vector<std::string>>();

            //check threshold of tasks
            if (files.size() > MAX_TASK_COUNT_PER_REQ)
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "too many tasks, please provide less than 10 tasks each time.";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            if (!CONNECTION_MANAGER->have_active_channel())
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "dbc node do not connect to network, pls check.";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            //send to net
            for (auto &file : files)
            {
                fs::path task_file_path = fs::system_complete(fs::path(file.c_str()));
                if (false == fs::exists(task_file_path) || (false == fs::is_regular_file(task_file_path)))
                {
                    LOG_ERROR << "ai power requester service file not exists or error: " << file;

                    ai::dbc::cmd_task_info task_info;
                    task_info.create_time = time(nullptr);
                    task_info.result = file + ": task config file error";

                    cmd_resp->task_info_list.push_back(task_info);

                    continue;
                }
                ai::dbc::cmd_task_info task_info;
                auto req_msg = create_task_msg(file, cmd_req_content->single_task_config_opts, task_info);

                if (nullptr == req_msg)
                {
                    LOG_ERROR << "ai power requestor service create task msg from file error: " << file;

//                    task_info.create_time = time(nullptr);
                    task_info.result = file + " " + task_info.result;

                    cmd_resp->task_info_list.push_back(task_info);
                }
                else
                {
                    if (E_SUCCESS != CONNECTION_MANAGER->broadcast_message(req_msg))
                    {
                        //ai::dbc::cmd_task_info task_info;
                        task_info.create_time = time(nullptr);
                        task_info.result = file + "broad cast error.";

                        cmd_resp->task_info_list.push_back(task_info);
                        continue;
                    }

                    std::shared_ptr<matrix::service_core::start_training_req> req_content = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(req_msg->content);
                    assert(nullptr != req_content);

                    ai::dbc::cmd_task_info task_info;
                    task_info.__set_create_time(std::time(nullptr));
                    task_info.__set_task_id(req_content->body.task_id);
                    task_info.__set_status(task_unknown);

                    cmd_resp->task_info_list.push_back(task_info);

                    //flush to db
                    write_task_info_to_db(task_info);
                }
            }

            //public resp directly
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_multi_training_resp).name(), cmd_resp);

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
            if (!is_task_exist_in_db(task_id))
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

            std::string message = req_content->body.task_id + req_content->header.nonce;
            std::string sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());
            if (sign.empty())
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "sign error. pls check node key or task property";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);
                return E_DEFAULT;
            }

            std::map<std::string, std::string> extern_info;
            extern_info["sign"] = sign;
            extern_info["sign_algo"] = ECDSA;
            extern_info["origin_id"] = CONF_MANAGER->get_node_id();
            req_content->header.__set_exten_info(extern_info);

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
                if (!read_task_info_from_db(cmd_req->task_list, vec_task_infos))
                {
                    LOG_ERROR << "load task info failed.";
                }
            }
            else
            {
                if (!read_task_info_from_db(vec_task_infos))
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
                    if (info.status & (task_stopped | task_succefully_closed | task_abnormally_closed | task_overdue_closed))
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

        int32_t ai_power_requestor_service::on_list_training_resp(std::shared_ptr<message> &msg)
        {

            if (!msg)
            {
                LOG_ERROR << "recv list_training_resp but msg is nullptr";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::list_training_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(msg->content);


            if (!rsp_content)
            {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(rsp_content->header.nonce) != true)
            {
                LOG_ERROR << "ai power requster service on_list_training_resp. nonce error ";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(rsp_content->header.session_id) != true)
            {
                LOG_ERROR << "ai power requster service on_list_training_resp. session_id error ";
                return E_DEFAULT;
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
            if( req_content->header.session_id == rsp_content->header.session_id )
            {
                LOG_ERROR << "ai power requester service sessionid not match: "
                             <<req_content->header.session_id
                             <<" : "
                             <<rsp_content->header.session_id;
            }


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
                    //if (it->second & (task_stopped | task_succefully_closed | task_abnormally_closed | task_overdue_closed))
                    if (info.status >= task_stopped)
                    {
                        write_task_info_to_db(info);
                    }
                    else
                    {
                        // fetch old status value from db
                        ai::dbc::cmd_task_info task_info_in_db;
                        if(read_task_info_from_db(info.task_id, task_info_in_db))
                        {
                            if(task_info_in_db.status != info.status && info.status != task_unknown)
                            {
                                write_task_info_to_db(info);
                            }
                        }
                    }
                }

                cmd_task_status cts;
                cts.task_id = info.task_id;
                cts.status = info.status;
                cts.create_time = info.create_time;
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
                    auto it = task_ids->find(info.task_id);
                    if (it != task_ids->end())
                    {
                        cts.status = it->second;
                        //update to db
                        info.status = it->second;
                        //if (it->second & (task_stopped | task_succefully_closed | task_abnormally_closed | task_overdue_closed))
                        if (info.status >= task_stopped)
                        {
                            write_task_info_to_db(info);
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
                req_content->body.__set_task_id(id_generator().generate_task_id());
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
            }
            catch (const std::exception &e)
            {
                LOG_ERROR << "keys missing" ;
                task_info.result = "parameters error: " + std::string(e.what());
                return nullptr;
            }

            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
            req_msg->set_content(req_content);

            std::string message = req_content->body.task_id + req_content->body.code_dir + req_content->header.nonce;
            std::string sign = id_generator().sign(message, CONF_MANAGER->get_node_private_key());
            if (sign.empty())
            {
                task_info.result = "sign error.pls check node key or task property";
                return nullptr;
            }

            task_info.task_id = req_content->body.task_id;

            std::map<std::string, std::string> extern_info;
            extern_info["sign"] = sign;
            extern_info["sign_algo"] = ECDSA;
            extern_info["origin_id"] = CONF_MANAGER->get_node_id();
            req_content->header.__set_exten_info(extern_info);

            return req_msg;
        }




        std::shared_ptr<message> ai_power_requestor_service::create_task_msg(const std::string &task_file, const bpo::options_description &opts,
                                                                             ai::dbc::cmd_task_info & task_info)
        {
            bpo::variables_map vm;

            try
            {
                std::ifstream conf_task(task_file);
                bpo::store(bpo::parse_config_file(conf_task, opts), vm);
                bpo::notify(vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);

                task_info.result = std::string("parse ai training task error: ") + diagnostic_information(e);

                return nullptr;
            }

            return create_task_msg(vm,task_info);
        }

        bool ai_power_requestor_service::write_task_info_to_db(ai::dbc::cmd_task_info &task_info)
        {
            if (!m_req_training_task_db || task_info.task_id.empty())
            {
                LOG_ERROR << "null ptr or null task_id.";
                return false;
            }

            //serialization
            std::shared_ptr<byte_buf> out_buf = std::make_shared<byte_buf>();
            binary_protocol proto(out_buf.get());
            task_info.write(&proto);

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;

            leveldb::Slice slice(out_buf->get_read_ptr(), out_buf->get_valid_read_len());
            leveldb::Status s = m_req_training_task_db->Put(write_options, task_info.task_id, slice);
            if (!s.ok())
            {
                LOG_ERROR << "ai power requestor service write task to db, task id: " << task_info.task_id << " failed: " << s.ToString();
                return false;
            }

            LOG_INFO << "ai power requestor service write task to db, task id: " << task_info.task_id;
            return true;
        }

        bool ai_power_requestor_service::read_task_info_from_db(std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status/* = 0*/)
        {
            if (!m_req_training_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }
            task_infos.clear();

            //read from db
            try
            {
                std::unique_ptr<leveldb::Iterator> it;
                it.reset(m_req_training_task_db->NewIterator(leveldb::ReadOptions()));
                for (it->SeekToFirst(); it->Valid(); it->Next())
                {
                    //deserialization
                    std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                    buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
                    binary_protocol proto(buf.get());
                    ai::dbc::cmd_task_info t_info;
                    t_info.read(&proto);

                    if (0 == (filter_status & t_info.status))
                    {
                        task_infos.push_back(std::move(t_info));
                        LOG_DEBUG << "ai power requestor service read task: " << t_info.task_id;
                    }
                }
            }
            catch (...)
            {
                LOG_ERROR << "ai power requestor service read task: broken data format";
                return false;
            }

            return !task_infos.empty();
        }

        bool ai_power_requestor_service::read_task_info_from_db(std::list<std::string> task_ids, std::vector<ai::dbc::cmd_task_info> &task_infos, uint32_t filter_status/* = 0*/)
        {
            if (!m_req_training_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }
            task_infos.clear();
            if (task_ids.empty())
            {
                LOG_WARNING << "not specify task id.";
                return false;
            }

            //read from db
            for (auto task_id : task_ids)
            {
                std::string task_value;
                leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
                if (!status.ok())
                {
                    LOG_ERROR << "read task(" << task_id << ") failed: " << status.ToString();
                    continue;
                }

                //deserialization
                std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
                binary_protocol proto(buf.get());
                ai::dbc::cmd_task_info t_info;
                t_info.read(&proto);
                if (0 == (filter_status & t_info.status))
                {
                    task_infos.push_back(std::move(t_info));
                    LOG_DEBUG << "ai power requestor service read task: " << t_info.task_id;
                }
            }

            return !task_infos.empty();
        }

        bool ai_power_requestor_service::read_task_info_from_db(std::string task_id, ai::dbc::cmd_task_info &task_info)
        {
            if (task_id.empty() || !m_req_training_task_db)
            {
                return false;
            }

            try
            {
                std::string task_value;
                leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
                if (!status.ok() || status.IsNotFound())
                {
                    return false;
                }

                //deserialization
                std::shared_ptr<byte_buf> buf = std::make_shared<byte_buf>();
                buf->write_to_byte_buf(task_value.data(), (uint32_t)task_value.size());
                binary_protocol proto(buf.get());
                task_info.read(&proto);
            }
            catch (...)
            {
                LOG_ERROR << "failed to read task info from db";
                return false;
            }

            return true;
        }

        bool ai_power_requestor_service::is_task_exist_in_db(std::string task_id)
        {
            if (task_id.empty() || !m_req_training_task_db)
            {
                return false;
            }

            std::string task_value;
            leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), task_id, &task_value);
            if (!status.ok() || status.IsNotFound())
            {
                return false;
            }

            return true;
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
            std::string task_value;
            leveldb::Status status = m_req_training_task_db->Get(leveldb::ReadOptions(), cmd_req->task_id, &task_value);
            if (!status.ok())
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

        int32_t ai_power_requestor_service::on_logs_resp(std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "recv logs_resp but msg is nullptr";
                return E_DEFAULT;
            }

            std::shared_ptr<matrix::service_core::logs_resp> rsp_content = std::dynamic_pointer_cast<matrix::service_core::logs_resp>(msg->content);
            if (!rsp_content)
            {
                LOG_ERROR << "recv logs_resp but ctn is nullptr";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(rsp_content->header.nonce) != true)
            {
                LOG_DEBUG << "ai power requster service on_logs_resp. nonce error ";
                return E_DEFAULT;
            }

            if (id_generator().check_base58_id(rsp_content->header.session_id) != true)
            {
                LOG_DEBUG << "ai power requster service on_logs_resp. session_id error ";
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
            log.log_content = std::move(rsp_content->body.log.log_content);


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

        //int32_t ai_power_requestor_service::on_cmd_clear(const std::shared_ptr<message> &msg)
        //{
        //    fs::path task_db_path = env_manager::get_db_path();
        //    leveldb::Options options;
        //
        //    options.create_if_missing = true;
        //    task_db_path /= fs::path("req_training_task.db");
        //
        //    if (true == fs::is_directory(task_db_path))
        //    {
        //        leveldb::Status s = leveldb::DestroyDB(task_db_path.generic_string(), options);

        //        if (false == s.ok())
        //        {
        //            LOG_DEBUG << "ai power requestor service get logs timer time out remove session: ";
        //        }
        //    }

        //    //publish cmd resp
        //    std::shared_ptr<ai::dbc::cmd_clear_resp> cmd_resp = std::make_shared<ai::dbc::cmd_clear_resp>();


        //    //return cmd resp
        //    TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_clear_resp).name(), cmd_resp);

        //    return E_SUCCESS;
        //}

        int32_t ai_power_requestor_service::on_cmd_ps(const std::shared_ptr<message> &msg)
        {
            if (!msg)
            {
                LOG_ERROR << "recv logs_resp but msg is nullptr";
                return E_DEFAULT;
            }

            auto cmd_req = std::dynamic_pointer_cast<cmd_ps_req>(msg->get_content());
            if (!cmd_req)
            {
                LOG_ERROR << "recv logs_resp but ctn is nullptr";
                return E_DEFAULT;
            }
            std::vector<ai::dbc::cmd_task_info>  task_infos;
            if (cmd_req->task_id == "all")
            {
                read_task_info_from_db(task_infos);
            }
            else
            {
                std::vector<std::string> vec;
                string_util::split(cmd_req->task_id, ",", vec);
                std::list<std::string> task_ids;
                std::copy(vec.begin(), vec.end(), std::back_inserter(task_ids));
                read_task_info_from_db(task_ids, task_infos);
            }


            //publish cmd resp
            std::shared_ptr<ai::dbc::cmd_ps_resp> cmd_resp = std::make_shared<ai::dbc::cmd_ps_resp>();
            COPY_MSG_HEADER(cmd_req,cmd_resp);
            cmd_resp->task_infos = task_infos;
            //return cmd resp
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_ps_resp).name(), cmd_resp);

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
            if (!read_task_info_from_db(vec_task_infos))
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
                    bool cleanable = (cmd_req_content->clean_all) ||
                        (cmd_req_content->task_id.empty() && task.status == task_unknown) ||
                        (cmd_req_content->task_id == task.task_id);

                    if (cleanable)
                    {
                        LOG_INFO << "delete task " << task.task_id;
                        m_req_training_task_db->Delete(leveldb::WriteOptions(), task.task_id);

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
