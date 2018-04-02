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
#include "ai_power_service.h"
#include "p2p_net_service.h"
#include "peer_node.h"



using namespace std;
using namespace boost::asio::ip;
using namespace matrix::core;
using namespace ai::dbc;


namespace matrix
{
	namespace service_core
	{

		int32_t ai_power_service::init_conf()
		{
			return E_SUCCESS;
		}

		int32_t ai_power_service::service_init(bpo::variables_map &options)
		{
			int32_t ret = E_SUCCESS;

			init_subscription();

			init_invoker();

			return E_SUCCESS;
		}

		void ai_power_service::init_subscription()
		{
			TOPIC_MANAGER->subscribe(AI_TRAINGING_NOTIFICATION_RESP, [this](std::shared_ptr<message> &msg) {return send(msg); });
		}

		void ai_power_service::init_invoker()
		{
			invoker_type invoker;

			invoker = std::bind(&ai_power_service::on_start_training_resp, this, std::placeholders::_1);
			m_invokers.insert({ AI_TRAINGING_NOTIFICATION_RESP,{ invoker } });

		}


		int32_t ai_power_service::on_start_training_resp(std::shared_ptr<message> &msg)
		{
			std::shared_ptr<base> content = msg->get_content();
			std::shared_ptr<matrix::service_core::start_training_req> req = std::dynamic_pointer_cast<matrix::service_core::start_training_req>(content);
			//verify arguments
			//

		    //start docker option
			return E_SUCCESS;
		}

	}

}