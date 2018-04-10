/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºdbc_server_initiator.cpp
* description    £ºdbc server initiator for dbc core
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#include "dbc_server_initiator.h"
#include "server.h"
#include "dbc_conf_manager.h"
#include "connection_manager.h"
#include "env_manager.h"
#include "topic_manager.h"
#include "version.h"
#include "p2p_net_service.h"
#include "ai_power_service_requestor.h"
#include "ai_power_service_provider.h"
#include "cmd_line_service.h"

using namespace std::chrono;
using namespace matrix::core;
using namespace matrix::service_core;
using namespace ai::dbc;


extern std::chrono::high_resolution_clock::time_point server_start_time;


namespace ai
{
	namespace dbc
	{

		int32_t dbc_server_initiator::init(int argc, char* argv[])
		{
			int32_t ret = E_SUCCESS;
			LOG_DEBUG << "begin to init dbc core";

			//parse command line
			LOG_DEBUG << "begin to init command line";
			variables_map vm;
			ret = parse_command_line(argc, argv, vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			LOG_DEBUG << "parse command line succefully";

			std::shared_ptr<matrix::core::module> mdl(nullptr);

			//env_manager
			LOG_DEBUG << "begin to init env manager";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<env_manager>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init env manager succefully";

			//core conf_manager
			LOG_DEBUG << "begin to init conf manager";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<conf_manager>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init conf manager succefully";

			//topic_manager
			LOG_DEBUG << "begin to init topic manager";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<topic_manager>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init topic manager succefully";

			//connection_manager
			LOG_DEBUG << "begin to init connection manager";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<connection_manager>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init connection manager succefully";

			//p2p net service
			LOG_DEBUG << "begin to init p2p net service";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<matrix::service_core::p2p_net_service>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init p2p net service succefully";

			LOG_DEBUG << "begin to init ai_power_service_requestor";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<matrix::service_core::ai_power_service_requestor>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init p2p ai_power_service_requestor succefully";


			LOG_DEBUG << "begin to init ai_power_service_provider";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<ai_power_service_provider>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init ai_power_service_provider succefully";

			//cmd line service
			LOG_DEBUG << "begin to init comand line service";
			mdl = std::dynamic_pointer_cast<module>(std::make_shared<ai::dbc::cmd_line_service>());
			g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
			ret = mdl->init(vm);
			if (E_SUCCESS != ret)
			{
				//logging
				return ret;
			}
			mdl->start();
			LOG_DEBUG << "init comand line service succefully";

			//log cost time
			high_resolution_clock::time_point init_end_time = high_resolution_clock::now();
			auto time_span_ms = duration_cast<milliseconds>(init_end_time - server_start_time);
			LOG_DEBUG << "init dbc core successfully, cost time: " << time_span_ms.count() << " ms";

			return E_SUCCESS;
		}

		int32_t dbc_server_initiator::exit()
		{
			return E_SUCCESS;
		}

		//parse command line
		int32_t dbc_server_initiator::parse_command_line(int argc, const char* const argv[], boost::program_options::variables_map &vm)
		{
			options_description opts("dbc command options");
			opts.add_options()
				("help,h", "get dbc core help info")
				("version,v", "get core version info");

			try
			{
				//parse
				bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
				bpo::notify(vm);

				//help
				if (vm.count("help") || vm.count("h"))
				{
					cout << opts;
					return E_EXIT_PARSE_COMMAND_LINE;
				}
				//version
				else if (vm.count("version") || vm.count("v"))
				{
					cout << CORE_VERSION << endl;
					return E_EXIT_PARSE_COMMAND_LINE;
				}
				//ignore
				else
				{
					return E_SUCCESS;
				}
			}
			catch (...)
			{
				cout << argv[0] << " invalid command option" << endl;
				cout << opts;
			}

			return E_BAD_PARAM;
		}

	}

}





