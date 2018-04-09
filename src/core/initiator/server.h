/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：server.h
* description    ：core is abstracted into server, server is responsible for init and exit
* date                  : 2017.01.23
* author            ：Bruce Feng
**********************************************************************************/

#pragma once

#include <memory>
#include <functional>
#include "common.h"
#include "conf_manager.h"
#include "topic_manager.h"
#include "connection_manager.h"
#include "p2p_net_service.h"
#include "ai_power_service.h"
#include "ai_model_service.h"
#include "module_manager.h"
#include "server_initiator_factory.h"

using namespace std;
using namespace matrix::service_core;
using namespace ai::dbc;


#define TOPIC_MANAGER                               (g_server->get_topic_manager())
#define CONF_MANAGER                                (g_server->get_conf_manager())
#define CONNECTION_MANAGER                  (g_server->get_connection_manager())
#define P2P_SERVICE                                      (g_server->get_p2p_net_service())



namespace matrix
{
	namespace core
	{

		class server;
		class conf_manager;
		class server_initiator;
		class server_initiator_factory;
		class topic_manager;
		class connection_manager;
		class p2p_net_service;

		extern std::unique_ptr<server> g_server;

		class server
		{

			using init_factory = server_initiator_factory;
			using idle_task_functor_type = std::function<void()>;

		public:

			server() : m_init_result(E_SUCCESS), m_exited(false), m_module_manager(new module_manager()) {}

			virtual ~server() = default;

			virtual int32_t init(int argc, char* argv[]);

			virtual int32_t idle();

			virtual int32_t exit();

			virtual bool is_exited() { return m_exited; }

			virtual void set_exited(bool exited = true) { m_exited = exited; }

			virtual bool is_init_ok() { return (m_init_result == E_SUCCESS); }

			void bind_idle_task(idle_task_functor_type functor) { m_idle_task = functor; }

			shared_ptr<module_manager> get_module_manager() { return m_module_manager; }

			conf_manager *get_conf_manager() { return (conf_manager *)(m_module_manager->get(conf_manager_name).get()); }

			topic_manager *get_topic_manager() { return (topic_manager *)(m_module_manager->get(topic_manager_name).get()); }

			connection_manager *get_connection_manager() { return (connection_manager *)(m_module_manager->get(connection_manager_name).get()); }

			p2p_net_service * get_p2p_net_service() { return (p2p_net_service *)(m_module_manager->get(p2p_manager_name).get()); }
			ai_power_service * get_ai_power_service() { return (ai_power_service *)(m_module_manager->get(ai_power_service_manager_name).get()); }
			ai_model_service * get_ai_model_service() { return (ai_model_service *)(m_module_manager->get(ai_model_service_manager_name).get()); }


		protected:

			virtual void do_cycle_task();

		protected:

			bool m_exited;

			int32_t m_init_result;

			shared_ptr<server_initiator> m_initiator;

			shared_ptr<module_manager> m_module_manager;

			init_factory m_init_factory;

			idle_task_functor_type m_idle_task;

		};

	}

}



