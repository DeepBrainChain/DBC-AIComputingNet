/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : data_query_service.h
* description       : 
* date              : 2018/6/13
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once


#include <boost/asio.hpp>
#include <string>
#include "service_module.h"
#include "handler_create_functor.h"
#include "node_info_message.h"
#include "service_info_collection.h"
#include "node_info_collection.h"

using namespace std;
#ifdef WIN32
using namespace stdext;
#endif
using namespace matrix::core;
using namespace boost::asio::ip;

namespace service
{
    namespace misc
    {
        class data_query_service : public service_module
        {
        public:
            data_query_service() = default;

            ~data_query_service() override = default;

            std::string module_name() const override { return "net_query"; }

            int32_t init(bpo::variables_map &options) override;

        protected:
            enum {
                MAX_NAME_STR_LENGTH = 16
            };

            void init_subscription() override;

            void init_invoker() override;

            void init_timer() override;

            int32_t on_timer_node_info_collection(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_timer_service_broadcast(std::shared_ptr<matrix::core::core_timer> timer);

            int32_t on_guard_timer_expired_for_node_info_query(std::shared_ptr<core_timer> timer);

            void cfg_own_node(bpo::variables_map &options, std::string own_id);

            int32_t on_get_task_queue_size_resp(std::shared_ptr<dbc::network::message> &msg);

        private:
            int32_t on_cli_show_req(std::shared_ptr<dbc::network::message> &msg);

            int32_t on_net_show_req(std::shared_ptr<dbc::network::message> &msg);
            int32_t on_net_show_resp(std::shared_ptr<dbc::network::message> &msg);
            int32_t on_net_service_broadcast_req(std::shared_ptr<dbc::network::message> &msg);

            int32_t create_data_query_session(std::string session_id, std::shared_ptr<node_info_query_req_msg> q);
            void rm_data_query_session(std::shared_ptr<service_session> session);

        private:
            bool m_is_computing_node;
            std::string m_own_node_id;

            service_info_collection m_service_info_collection;
            node_info_collection m_node_info_collection;
        };
    }
}
