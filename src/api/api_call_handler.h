/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        : api_call_handler.h
* description    : api call handler for command line, json rpc
* date                  : 2018.03.25
* author             : Bruce Feng
**********************************************************************************/

#pragma once

#include <list>
#include <boost/program_options.hpp>
#include "service_message.h"
#include "callback_wait.h"
#include "server.h"
#include "matrix_types.h"
#include "console_printer.h"
#include "task_common_def.h"

#include "api_call.h"

using namespace std;
using namespace boost::program_options;
using namespace matrix::core;


#define DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS                 std::chrono::milliseconds(30000)                    //unit: ms

#define LIST_ALL_TASKS                                                                        0
#define LIST_SPECIFIC_TASKS                                                              1


namespace ai
{
    namespace dbc
    {

        using ::dbc::cmd_get_peer_nodes_req;
        using ::dbc::cmd_get_peer_nodes_resp;

        class outputter
        {
        public:

            void format_output() {}
        };

        class cmd_start_training_req : public matrix::core::base
        {
        public:
            std::string task_file_path;
        };

        class cmd_start_training_resp : public matrix::core::base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            std::string task_id;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "task id: " << task_id << endl;
            }
        };

        class cmd_stop_training_req : public matrix::core::base
        {
        public:
            std::string task_id;
        };

        class cmd_stop_training_resp : public matrix::core::base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "stopping training task......" << endl;
            }
        };

        class cmd_start_multi_training_req : public matrix::core::base
        {
        public:
            std::string mulit_task_file_path;
        };

        class cmd_start_multi_training_resp : public matrix::core::base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            std::list<std::string> task_list;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "task_id:" << endl;

                auto it = task_list.begin();
                for (; it != task_list.end(); it++)
                {
                    cout << *it << endl;
                }
            }
        };

        class cmd_list_training_req : public matrix::core::base
        {
        public:

            int8_t list_type;                                   //0: list all tasks; 1: list specific tasks

            std::list<std::string> task_list;
        };

        class cmd_task_status
        {
        public:

            std::string task_id;

            int8_t status;
        };

        class cmd_list_training_resp : public matrix::core::base, public outputter
        {
        public:

            int32_t result;
            std::string result_info;

            std::list<cmd_task_status> task_status_list;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }
            
                console_printer printer;
                printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 10);

                printer << matrix::core::init << "task_id" << "task_status" << matrix::core::endl;
                for (auto it = task_status_list.begin(); it != task_status_list.end(); it++)
                {
                    printer << matrix::core::init << it->task_id << to_training_task_status_string(it->status) << matrix::core::endl;
                }
            }
        };


  class cmd_get_peer_nodes_resp_formater : public outputter
        {

        public:
            cmd_get_peer_nodes_resp_formater(std::shared_ptr<cmd_get_peer_nodes_resp> data){
                m_data = data;
            };

            void format_output()
            {
                console_printer printer;
                printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 64)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);

                printer << matrix::core::init << "peer_id" << "time_stamp" << "ip" << "port" << "service_list" << matrix::core::endl;

                auto v = m_data.get();
                if (v == nullptr)
                {
                    return;
                }

                auto it = v->peer_nodes_list.begin();
                for (; it != v->peer_nodes_list.end(); it++)
                {
                    std::string service_list;  //left to later
                    printer << matrix::core::init << it->peer_node_id << it->live_time_stamp << it->addr.ip << it->addr.port << service_list << matrix::core::endl;
                }
            }
        private:
            std::shared_ptr<cmd_get_peer_nodes_resp> m_data;

        };

//  move to p2p_net/api_call.h
//        class cmd_network_address
//        {
//        public:
//
//            std::string ip;
//
//            uint16_t port;
//        };
//
//        class cmd_peer_node_info
//        {
//        public:
//
//            std::string peer_node_id;
//
//            int32_t live_time_stamp;
//
//            cmd_network_address addr;
//
//            std::vector<std::string> service_list;
//
//            cmd_peer_node_info& operator=(const matrix::service_core::peer_node_info &info)
//            {
//                peer_node_id = info.peer_node_id;
//                live_time_stamp = info.live_time_stamp;
//                addr.ip = info.addr.ip;
//                addr.port = info.addr.port;
//                service_list = info.service_list;
//                return *this;
//            }
//        };

//        class cmd_get_peer_nodes_req : public matrix::core::base
//        {
//
//        };
//
//        class cmd_get_peer_nodes_resp : public matrix::core::base, public outputter
//        {
//        public:
//            int32_t result;
//            std::string result_info;
//
//            std::vector<cmd_peer_node_info> peer_nodes_list;
//
//            void format_output()
//            {
//                console_printer printer;
//                printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 64)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);
//
//                printer << matrix::core::init << "peer_id" << "time_stamp" << "ip" << "port" << "service_list" << matrix::core::endl;
//
//                auto it = peer_nodes_list.begin();
//                for (; it != peer_nodes_list.end(); it++)
//                {
//                    std::string service_list;  //left to later
//                    printer << matrix::core::init << it->peer_node_id << it->live_time_stamp << it->addr.ip << it->addr.port << service_list << matrix::core::endl;
//                }
//            }
//        };

        class cmd_logs_req : public matrix::core::base
        {
        public:

            std::string task_id;

            std::vector<std::string> peer_nodes_list;

            uint8_t head_or_tail;

            uint16_t number_of_lines;
        };

        class cmd_peer_node_log
        {
        public:

            std::string peer_node_id;

            std::string log_content;

        };

        class cmd_logs_resp : public matrix::core::base, public outputter
        {
        public:

            int32_t result;
            std::string result_info;

            std::vector<cmd_peer_node_log> peer_node_logs;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                auto it = peer_node_logs.begin();
                for (; it != peer_node_logs.end(); it++)
                {
                    cout << "****************************************************************************************************" << endl;
                    cout << "node id: " << it->peer_node_id << endl;
                    cout << it->log_content << endl;
                }
            }
        };

        class api_call_handler
        {
        public:

            api_call_handler() : m_wait(new callback_wait<>)
            {
                init_subscription();
            }

            ~api_call_handler() = default;

            void init_subscription();

            template<typename req_type, typename resp_type>
            std::shared_ptr<resp_type> invoke(std::shared_ptr<req_type> req)
            {
                //construct message
                std::shared_ptr<message> msg = std::make_shared<message>();
                msg->set_name(typeid(req_type).name());
                msg->set_content(std::dynamic_pointer_cast<base>(req));

                m_wait->reset();

                //publish
                TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);
                
                //synchronous wait for resp
                m_wait->wait_for(DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS);
                if (true == m_wait->flag())
                {
                    return std::dynamic_pointer_cast<resp_type>(m_resp);
                }
                else
                {
                    LOG_DEBUG << "api_call_handler time out: " << msg->get_name();
                    return nullptr;
                }

            }   

        private:

            //std::mutex m_mutex;           currently no need. 

            std::shared_ptr<matrix::core::base> m_resp;

            std::shared_ptr<callback_wait<>> m_wait;

        };

    }

}
