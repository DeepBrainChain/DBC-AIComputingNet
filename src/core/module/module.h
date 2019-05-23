/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   module.h
* description      :   module inteface
* date             :   2018.01.20
* author           :   Bruce Feng
**********************************************************************************/
#pragma once

#include "common.h"
#include <string>
#include <boost/program_options.hpp>
#include <memory>
#include "timer.h"

namespace bpo = boost::program_options;

extern const std::string default_name;
extern const std::string module_manager_name;
extern const std::string conf_manager_name;
extern const std::string env_manager_name;
extern const std::string topic_manager_name;
extern const std::string connection_manager_name;
extern const std::string p2p_service_name;
extern const std::string ai_power_requestor_service_name;
extern const std::string ai_power_provider_service_name;
extern const std::string common_service_name;

namespace matrix
{
	namespace core
	{

		class module
		{
		public:

			virtual ~module() {}

			virtual std::string module_name() const { return "default module"; };

			virtual int32_t init(bpo::variables_map &options) { return 0; }

			virtual int32_t start() { return 0; }

			virtual int32_t stop() { return 0; }

			virtual int32_t exit() { return 0; }

			virtual int32_t on_time_out(std::shared_ptr<core_timer> timer) { return 0; }

		};
	}
}
