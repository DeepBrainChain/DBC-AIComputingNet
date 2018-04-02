/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºp2p_net_service.h
* description    £ºp2p net service
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

		class ai_power_service : public service_module
		{
		public:

			ai_power_service() = default;

			virtual ~ai_power_service() = default;

			virtual std::string module_name() const { return ai_power_service_manager_name; }

		protected:

			int32_t init_conf();

			void init_subscription();

			void init_invoker();

			virtual int32_t service_init(bpo::variables_map &options);

		protected:

			int32_t on_start_training_resp(std::shared_ptr<message> &msg);

		protected:

			std::vector<std::string> task_id_set;

		};

	}

}



