/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºp2p_net_service.cpp
* description    £ºp2p net service
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
#include "ai_model_service.h"




using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;



namespace matrix
{
	namespace service_core
	{

		int32_t ai_model_service::init_conf()
		{
			return E_SUCCESS;
		}

		int32_t ai_model_service::service_init(bpo::variables_map &options)
		{
			int32_t ret = E_SUCCESS;

			init_subscription();

			init_invoker();

			return E_SUCCESS;
		}


		void ai_model_service::init_subscription()
		{
			TOPIC_MANAGER->subscribe(AI_TRAINGING_NOTIFICATION_REQ, [this](std::shared_ptr<message> &msg) {return send(msg); });
		}

		void ai_model_service::init_invoker()
		{
			invoker_type invoker;

			invoker = std::bind(&ai_model_service::on_start_training_req, this, std::placeholders::_1);
			m_invokers.insert({ AI_TRAINGING_NOTIFICATION_REQ,{ invoker } });

		}

		int32_t ai_model_service::on_start_training_req(std::shared_ptr<message> &msg)
		{
			std::shared_ptr<base> content = msg->get_content();
			std::shared_ptr<cmd_start_training_req> req = std::dynamic_pointer_cast<cmd_start_training_req>(content);
			assert(nullptr != req);

			bpo::variables_map vm;
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
				::store(bpo::parse_config_file(conf_task, task_config_opts), vm);
				bpo::notify(vm);
			}

			catch (const boost::exception & e)
			{
				LOG_ERROR << "task config parse local conf error: " << diagnostic_information(e);
			}

			std::shared_ptr<message> req_msg = std::make_shared<message>();
			std::shared_ptr<matrix::service_core::start_training_req> resp_content = std::make_shared<matrix::service_core::start_training_req>();

			resp_content->header.magic = TEST_NET;
			resp_content->header.msg_name = AI_TRAINGING_NOTIFICATION_RESP;
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

			req_msg->set_content(std::dynamic_pointer_cast<base>(resp_content));
			req_msg->set_name(AI_TRAINGING_NOTIFICATION_RESP);

			CONNECTION_MANAGER->broadcast_message(req_msg);

			return E_SUCCESS;
		}

	}

}