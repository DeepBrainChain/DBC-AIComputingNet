/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºai_power_requestor_service.cpp
* description    £ºai_power_requestor_service
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/
#include <cassert>
#include "server.h"
#include "api_call_handler.h"
#include "conf_manager.h"
#include "tcp_acceptor.h"
#include "service_message_id.h"
#include "service_message_def.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "matrix_client_socket_channel_handler.h"
#include "matrix_server_socket_channel_handler.h"
#include "handler_create_functor.h"
#include "channel.h"
#include "ip_validator.h"
#include "port_validator.h"
#include <boost/exception/all.hpp>
#include <iostream>
#include "ai_power_requestor_service.h"
#include "id_generator.h"




using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;



namespace matrix
{
    namespace service_core
    {
        ai_power_requestor_service::ai_power_requestor_service()
            : m_req_training_task_db()
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
            TOPIC_MANAGER->subscribe(typeid(cmd_start_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_start_training_req(msg); });

            //cmd start multi training
            TOPIC_MANAGER->subscribe(typeid(cmd_start_multi_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_start_multi_training_req(msg);});

            //cmd stop training
            TOPIC_MANAGER->subscribe(typeid(cmd_stop_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_stop_training_req(msg); });

            //cmd list training
            TOPIC_MANAGER->subscribe(typeid(cmd_list_training_req).name(), [this](std::shared_ptr<message> &msg) { return on_cmd_list_training_req(msg); });
            //list training resp
            TOPIC_MANAGER->subscribe(LIST_TRAINING_RESP, [this](std::shared_ptr<message> &msg) {return send(msg); });
        }

        void ai_power_requestor_service::init_invoker()
        {
            invoker_type invoker;

            //list training resp
            invoker = std::bind(&ai_power_requestor_service::on_list_training_resp, this, std::placeholders::_1);
            m_invokers.insert({ LIST_TRAINING_RESP,{ invoker } });

        }

        int32_t ai_power_requestor_service::init_db()
        {
            leveldb::DB *db = nullptr;
            leveldb::Options  options;
            options.create_if_missing = true;

            //get db path
            fs::path task_db_path = env_manager::get_db_path();
            task_db_path /= fs::path("req_training_task.db");
            LOG_DEBUG << "training task db path: " << task_db_path.generic_string();

            //open db
            leveldb::Status status = leveldb::DB::Open(options, task_db_path.generic_string(), &db);
            if (false == status.ok())
            {
                LOG_ERROR << "ai_power_service init training task db error";
                return E_DEFAULT;
            }

            //smart point auto close db
            m_req_training_task_db.reset(db);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_start_training_req(std::shared_ptr<message> &msg)
        {
            bpo::variables_map vm;

            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<cmd_start_training_req> req = std::dynamic_pointer_cast<cmd_start_training_req>(content);
            assert(nullptr != req && nullptr != content);

            std::shared_ptr<ai::dbc::cmd_start_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_id = "";

            //check file exist
            fs::path task_file_path(fs::initial_path());
            task_file_path = fs::system_complete(fs::path(req->task_file_path.c_str()));
            if (false == fs::exists(task_file_path) || false == fs::is_regular_file(task_file_path))
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "training task file does not exist";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //parse task config file
            bpo::options_description task_config_opts("task config file options");
            task_config_opts.add_options()
                ("task_id", bpo::value<std::string>(), "")
                ("select_mode", bpo::value<int8_t>()->default_value(0), "")
                ("master", bpo::value<std::string>(), "")
                ("peer_nodes_list", bpo::value<std::vector<std::string>>(), "")
                ("server_specification", bpo::value<std::string>(), "")
                ("server_count", bpo::value<int32_t>(), "")
                ("training_engine", bpo::value<int32_t>(), "")
                ("code_dir", bpo::value<std::string>(), "")
                ("entry_file", bpo::value<std::string>(), "")
                ("data_dir", bpo::value<std::string>(), "")
                ("checkpoint_dir", bpo::value<std::string>(), "")
                ("hyper_parameters", bpo::value<std::string>(), "");

            try
            {
                std::ifstream conf_task(req->task_file_path);
                bpo::store(bpo::parse_config_file(conf_task, task_config_opts), vm);
                bpo::notify(vm);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);

                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "parse ai training task error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //validate parameters
            if (E_DEFAULT == validate_cmd_training_task_conf(vm))
            {
                cmd_resp->result = E_DEFAULT;
                cmd_resp->result_info = "parse ai training task parameters error";
                TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

                return E_DEFAULT;
            }

            //prepare broadcast req
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::start_training_req> broadcast_req_content = std::make_shared<matrix::service_core::start_training_req>();

            broadcast_req_content->header.magic = TEST_NET;
            broadcast_req_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;

            broadcast_req_content->header.check_sum = 0;
            broadcast_req_content->header.session_id = 0;

            id_generator gen;
            broadcast_req_content->body.task_id = gen.generate_task_id();
            broadcast_req_content->body.select_mode = vm["select_mode"].as<int8_t>();
            broadcast_req_content->body.master = vm["master"].as<std::string>();
            broadcast_req_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
            broadcast_req_content->body.server_specification = vm["server_specification"].as<std::string>();
            broadcast_req_content->body.server_count = vm["server_count"].as<int32_t>();
            broadcast_req_content->body.training_engine = vm["training_engine"].as<int32_t>();
            broadcast_req_content->body.code_dir = vm["code_dir"].as<std::string>();
            broadcast_req_content->body.entry_file = vm["entry_file"].as<std::string>();
            broadcast_req_content->body.data_dir = vm["data_dir"].as<std::string>();
            broadcast_req_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
            broadcast_req_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();

            req_msg->set_content(std::dynamic_pointer_cast<base>(broadcast_req_content));
            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);

            CONNECTION_MANAGER->broadcast_message(req_msg);

            //peer won't reply, so public resp directly
            cmd_resp->result = E_SUCCESS;
            cmd_resp->task_id = broadcast_req_content->body.task_id;
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::validate_cmd_training_task_conf(const bpo::variables_map &vm)
        {
            if (0 == vm.count("select_mode")
                || 0 == vm.count("master")
                || 0 == vm.count("peer_nodes_list")
                || 0 == vm.count("server_specification")
                || 0 == vm.count("server_count")
                || 0 == vm.count("training_engine")
                || 0 == vm.count("code_dir")
                || 0 == vm.count("entry_file")
                || 0 == vm.count("data_dir")
                || 0 == vm.count("checkpoint_dir")
                || 0 == vm.count("hyper_parameters")
                )
            {
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        int32_t ai_power_requestor_service::on_cmd_start_multi_training_req(std::shared_ptr<message> &msg)
        {
            std::shared_ptr<base> content = msg->get_content();
            std::shared_ptr<cmd_start_multi_training_req> req = std::dynamic_pointer_cast<cmd_start_multi_training_req>(content);
            assert(nullptr != req);
            if (!req)
            {
                LOG_ERROR << "null msg";
                return E_DEFAULT;
            }
            //cmd resp
            std::shared_ptr<ai::dbc::cmd_start_multi_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_start_multi_training_resp>();

            bpo::options_description opts("task config file options");
            add_task_config_opts(opts);

            std::vector<std::string> files;
            string_util::split(req->mulit_task_file_path, ",", files);
            for (auto &file : files) {
                auto req_msg = create_task_msg_from_file(file, opts);
                CONNECTION_MANAGER->broadcast_message(req_msg);
                cmd_resp->task_list.push_back(std::dynamic_pointer_cast<matrix::service_core::start_training_req>(req_msg->content)->body.task_id);
            }

            //public resp directly            
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_start_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_stop_training_req(const std::shared_ptr<message> &msg)
        {
            const std::string &task_id = std::dynamic_pointer_cast<cmd_stop_training_req>(msg->get_content())->task_id;
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::stop_training_req> req_content = std::make_shared<matrix::service_core::stop_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = STOP_TRAINING_REQ;
            req_content->header.check_sum = 0;
            req_content->header.session_id = 0;
            //body
            req_content->body.task_id = task_id;

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
            CONNECTION_MANAGER->broadcast_message(req_msg);

            //there's no reply, so public resp directly
            std::shared_ptr<ai::dbc::cmd_stop_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_stop_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_stop_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        int32_t ai_power_requestor_service::on_cmd_list_training_req(const std::shared_ptr<message> &msg)
        {
            auto cmd_req = std::dynamic_pointer_cast<cmd_list_training_req>(msg->get_content());
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            auto req_content = std::make_shared<matrix::service_core::list_training_req>();
            //header
            req_content->header.magic = TEST_NET;
            req_content->header.msg_name = LIST_TRAINING_REQ;
            //matrix::core::id_generator  id_gen;
            req_content->header.check_sum = 0;//id_gen.generate_check_sum();
            req_content->header.session_id = 0;// id_gen.generate_session_id();
                                               //body
            if (cmd_req->list_type == 1) //0: list all tasks; 1: list specific tasks
            {
                req_content->body.task_list.assign(cmd_req->task_list.begin(), cmd_req->task_list.end());
            }
            else
            {
                read_task_info_from_db(req_content->body.task_list);
            }

            req_msg->set_content(std::dynamic_pointer_cast<base>(req_content));
            req_msg->set_name(req_content->header.msg_name);
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
            std::shared_ptr<matrix::service_core::list_training_resp> rsp_ctn = std::dynamic_pointer_cast<matrix::service_core::list_training_resp>(msg->content);
            if (!rsp_ctn)
            {
                LOG_ERROR << "recv list_training_resp but ctn is nullptr";
                return E_DEFAULT;
            }
            //broadcast resp
            CONNECTION_MANAGER->broadcast_message(msg);

            //public cmd resp
            std::shared_ptr<ai::dbc::cmd_list_training_resp> cmd_resp = std::make_shared<ai::dbc::cmd_list_training_resp>();
            cmd_resp->result = E_SUCCESS;
            cmd_resp->result_info = "";
            for (auto ts : rsp_ctn->body.task_status_list)
            {
                LOG_DEBUG << "recv list_training_resp: " << ts.task_id << " : " << ts.status;
                cmd_task_status cts;
                cts.task_id = ts.task_id;
                cts.status = ts.status;
                cmd_resp->task_status_list.push_back(std::move(cts));
            }
            TOPIC_MANAGER->publish<void>(typeid(ai::dbc::cmd_list_training_resp).name(), cmd_resp);

            return E_SUCCESS;
        }

        void ai_power_requestor_service::add_task_config_opts(bpo::options_description &opts) const
        {
            opts.add_options()
                ("task_id", bpo::value<std::string>(), "")
                ("select_mode", bpo::value<int8_t>()->default_value(0), "")
                ("master", bpo::value<std::string>(), "")
                ("peer_nodes_list", bpo::value<std::vector<std::string>>(), "")
                ("server_specification", bpo::value<std::string>(), "")
                ("server_count", bpo::value<int32_t>(), "")
                ("training_engine", bpo::value<int32_t>(), "")
                ("code_dir", bpo::value<std::string>(), "")
                ("entry_file", bpo::value<std::string>(), "")
                ("data_dir", bpo::value<std::string>(), "")
                ("checkpoint_dir", bpo::value<std::string>(), "")
                ("hyper_parameters", bpo::value<std::string>(), "");
        }

        std::shared_ptr<message> ai_power_requestor_service::create_task_msg_from_file(const std::string &task_file, const bpo::options_description &opts)
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
            }
            std::shared_ptr<message> req_msg = std::make_shared<message>();
            std::shared_ptr<matrix::service_core::start_training_req> resp_content = std::make_shared<matrix::service_core::start_training_req>();

            resp_content->header.magic = TEST_NET;
            resp_content->header.msg_name = AI_TRAINING_NOTIFICATION_REQ;
            resp_content->header.check_sum = 0;
            resp_content->header.session_id = 0;

            resp_content->body.task_id = vm["task_id"].as<std::string>();
            resp_content->body.select_mode = vm["select_mode"].as<int8_t>();
            resp_content->body.master = vm["master"].as<std::string>();
            resp_content->body.peer_nodes_list = vm["peer_nodes_list"].as<std::vector<std::string>>();
            resp_content->body.server_specification = vm["server_specification"].as<std::string>();
            resp_content->body.server_count = vm["server_count"].as<int32_t>();
            resp_content->body.training_engine = vm["training_engine"].as<int32_t>();
            resp_content->body.code_dir = vm["code_dir"].as<std::string>();
            resp_content->body.entry_file = vm["entry_file"].as<std::string>();
            resp_content->body.data_dir = vm["data_dir"].as<std::string>();
            resp_content->body.checkpoint_dir = vm["checkpoint_dir"].as<std::string>();
            resp_content->body.hyper_parameters = vm["hyper_parameters"].as<std::string>();

            req_msg->set_name(AI_TRAINING_NOTIFICATION_REQ);
            req_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
            return req_msg;
        }

        bool ai_power_requestor_service::write_task_info_to_db(std::string taskid)
        {
            if (!m_req_training_task_db || taskid.empty())
            {
                LOG_ERROR << "null ptr or null taskid.";
                return false;
            }

            //flush to db
            leveldb::WriteOptions write_options;
            write_options.sync = true;
            leveldb::Status s = m_req_training_task_db->Put(write_options, taskid, taskid);
            if (!s.ok())
            {
                LOG_ERROR << "write task(" << taskid << ") failed.";
                return false;
            }
            return true;
        }

        bool ai_power_requestor_service::read_task_info_from_db(std::vector<std::string> &vec)
        {
            if (!m_req_training_task_db)
            {
                LOG_ERROR << "level db not initialized.";
                return false;
            }

            //read from db
            std::string taskid;
            vec.clear();//necessary ?

            //iterate task in db
            std::unique_ptr<leveldb::Iterator> it;
            it.reset(m_req_training_task_db->NewIterator(leveldb::ReadOptions()));
            for (it->SeekToFirst(); it->Valid(); it->Next())
            {
                taskid = it->key().ToString();
                vec.push_back(taskid);
            }

            return true;
        }

    }//service_core
}