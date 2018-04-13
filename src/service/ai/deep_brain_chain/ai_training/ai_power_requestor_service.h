/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºai_power_requestor_service.h
* description    £ºai_power_requestor_service
* date                  : 2018.01.28
* author            £ºBruce Feng
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

		class ai_power_requestor_service : public service_module
		{
		public:

			ai_power_requestor_service() = default;

			virtual ~ai_power_requestor_service() = default;

			virtual std::string module_name() const { return ai_power_requestor_service_name; }

		protected:

			int32_t init_conf();

			void init_subscription();

			void init_invoker();

			virtual int32_t service_init(bpo::variables_map &options);

		protected:

			int32_t on_cmd_start_training_req(std::shared_ptr<message> &msg);
			int32_t on_cmd_start_multi_training_req(std::shared_ptr<message> &msg);

            int32_t on_cmd_stop_training_req(const std::shared_ptr<message> &msg);

            int32_t on_cmd_list_training_req(const std::shared_ptr<message> &msg);
            int32_t on_list_training_resp(std::shared_ptr<message> &msg);

        protected:
			void add_task_config_opts(bpo::options_description &opts) const;
			std::shared_ptr<message> create_task_msg_from_file(const std::string &task_file, const bpo::options_description &opts);

		protected:
			bpo::variables_map vm;
		};

	}

}

