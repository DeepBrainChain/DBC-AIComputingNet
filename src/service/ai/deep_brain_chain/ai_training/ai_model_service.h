/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ��p2p_net_service.h
* description    ��p2p net service
* date                  : 2018.01.28
* author            ��Bruce Feng
**********************************************************************************/
#pragma once


#include "service_module.h"
#include <boost/asio.hpp>
#include <string>

using namespace matrix::core;
using namespace boost::asio::ip;


namespace matrix
{
	namespace service_core
	{

		class ai_model_service : public service_module
		{
		public:

			ai_model_service() = default;

			virtual ~ai_model_service() = default;

			virtual std::string module_name() const { return ai_model_service_manager_name; }

		protected:

			int32_t init_conf();

			void init_subscription();

			void init_invoker();

			virtual int32_t service_init(bpo::variables_map &options);

		protected:

			int32_t cmd_on_start_training_req(std::shared_ptr<message> &msg);

        protected:
			void add_task_config_opts(bpo::options_description &opts) const;
			std::shared_ptr<message> create_task_msg_from_file(const std::string &task_file, const bpo::options_description &opts);

		protected:

			std::vector<std::string> task_id_set;
			bpo::variables_map vm;
		};

	}

}

