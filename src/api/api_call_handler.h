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
#include <set>
#include "time_util.h"
#include "db_types.h"

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

        using matrix::service_core::cmd_get_peer_nodes_req;
        using matrix::service_core::cmd_get_peer_nodes_resp;

        class outputter
        {
        public:

            void format_output() {}
        };

        class cmd_start_training_req : public matrix::core::msg_base
        {
        public:
            std::string task_file_path;
        };

        class cmd_start_training_resp : public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            cmd_task_info  task_info;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "task id: " << task_info.task_id << "       create_time: " << task_info.create_time << endl;
            }
        };

        class cmd_stop_training_req : public matrix::core::msg_base
        {
        public:
            std::string task_id;
        };

        class cmd_stop_training_resp : public matrix::core::msg_base, public outputter
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

        class cmd_start_multi_training_req : public matrix::core::msg_base
        {
        public:
            std::string mulit_task_file_path;
        };

        class cmd_start_multi_training_resp : public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            std::list<cmd_task_info> task_info_list;

            void format_output()
            {
                if (E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                auto it = task_info_list.begin();
                for (; it != task_info_list.end(); it++)
                {
                    if (!it->task_id.empty())
                    {
                        cout << "task id: " << it->task_id << "     create_time: " << it->create_time << endl;
                    }
                    else
                    {
                        cout << "task id: " << it->task_id << "     create_time: " << it->create_time 
                            << "result: " << it->result << endl;
                    }                    
                }
            }
        };

        class cmd_list_training_req : public matrix::core::msg_base
        {
        public:

            int8_t list_type;                                   //0: list all tasks; 1: list specific tasks

            std::list<std::string> task_list;
        };

        class cmd_task_status
        {
        public:

            std::string task_id;

            time_t  create_time;

            int8_t status;
        };

        class cmd_list_training_resp : public matrix::core::msg_base, public outputter
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
                printer(LEFT_ALIGN, 5)(LEFT_ALIGN, 56)(LEFT_ALIGN, 24)(LEFT_ALIGN, 20);

                printer << matrix::core::init << "num" << "task_id" << "time" << "task_status" << matrix::core::endl;
                int i = 1;
                for (auto it = task_status_list.begin(); it != task_status_list.end(); ++it, ++i)
                {
                    printer << matrix::core::init << i << it->task_id << time_util::time_2_str(it->create_time) 
                        << to_training_task_status_string(it->status) << matrix::core::endl;
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
                printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 32)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);

                printer << matrix::core::init << "peer_id" << "time_stamp" << "ip" << "port" << "service_list" << matrix::core::endl;

                auto v = m_data.get();
                if (v == nullptr)
                {
                    return;
                }

                auto it = v->peer_nodes_list.begin();
                for (; it != v->peer_nodes_list.end(); ++it)
                {
                    std::string svc_list;
                    for (auto its = it->service_list.begin(); its != it->service_list.end(); ++its)
                    {
                        svc_list += *its;
                        if (its + 1 != it->service_list.end())
                        {
                            svc_list += "|";
                        }
                    }
                    printer << matrix::core::init << it->peer_node_id << it->live_time_stamp << it->addr.ip << it->addr.port << svc_list << matrix::core::endl;
                }
            }
        private:
            std::shared_ptr<cmd_get_peer_nodes_resp> m_data;

        };

        class cmd_logs_req : public matrix::core::msg_base
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

        class cmd_logs_resp : public matrix::core::msg_base, public outputter
        {
        public:

            int32_t result;
            std::string result_info;

            std::vector<cmd_peer_node_log> peer_node_logs;
            std::set<char> char_filter =
            {
                /*0x00,0x01,0x02,*/0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13
                ,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,/*0x1b,*/0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
                ,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b
                ,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f
                ,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x61,0x62,0x63
                ,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77
                ,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f
            };
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
                    const char *p = (it->log_content).c_str();
                    size_t size = (it->log_content).size();
                    for (size_t i = 0; i < size; i++)
                    {
                        if (char_filter.find(*p) != char_filter.end())
                        {
                            cout << *p;
                        }
                        p++;
                    }
                }
                cout << "\n";
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
                msg->set_content(req);

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
