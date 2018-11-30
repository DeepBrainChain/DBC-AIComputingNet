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
#include "ai_db_types.h"
#include "peer_candidate.h"
#include "util.h"

#include "log.h"

using namespace std;
using namespace boost::program_options;
using namespace matrix::core;
using namespace matrix::service_core;


#define DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS                 std::chrono::milliseconds(10000)                    //unit: ms

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

            void format_output()
            {
            }
        };

        class cmd_start_training_req:public matrix::core::msg_base
        {
        public:
            std::string task_file_path;

            std::map<std::string, std::string> parameters;

            bpo::variables_map vm;
        };

        class cmd_start_training_resp:public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            cmd_task_info task_info;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "task id: " << task_info.task_id << "       create_time: " << time_util::time_2_str(
                        task_info.create_time) << endl;
            }
        };

        class cmd_stop_training_req:public matrix::core::msg_base
        {
        public:
            std::string task_id;
        };

        class cmd_stop_training_resp:public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "stopping training task......" << endl;
            }
        };

        class cmd_task_clean_req:public matrix::core::msg_base
        {
        public:
            std::string task_id;
            bool clean_all;
        };

        class cmd_task_clean_resp:public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                cout << "clean up training task......" << result_info << endl;
            }
        };

        class cmd_start_multi_training_req:public matrix::core::msg_base
        {
        public:
            std::string mulit_task_file_path;

            bpo::options_description single_task_config_opts;
            bpo::options_description multi_tasks_config_opts;
        };

        class cmd_start_multi_training_resp:public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            std::list<cmd_task_info> task_info_list;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                auto it = task_info_list.begin();
                for(; it != task_info_list.end(); it++)
                {
                    if(!it->task_id.empty())
                    {
                        cout << "task id: " << it->task_id << "     create_time: " << time_util::time_2_str(
                                it->create_time) << endl;
                    }
                    else
                    {
                        cout << "task id: " << it->task_id << "     create_time: " << time_util::time_2_str(
                                it->create_time) << " result: " << it->result << endl;
                    }
                }
            }
        };

        class cmd_list_training_req:public matrix::core::msg_base
        {
        public:

            int8_t list_type;                                   //0: list all tasks; 1: list specific tasks

            std::list<std::string> task_list;
        };

        class cmd_task_status
        {
        public:

            std::string task_id;

            time_t create_time;

            int8_t status;
        };

        class cmd_list_training_resp:public matrix::core::msg_base, public outputter
        {
        public:

            int32_t result;
            std::string result_info;

            std::list<cmd_task_status> task_status_list;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                console_printer printer;
                printer(LEFT_ALIGN, 5)(LEFT_ALIGN, 56)(LEFT_ALIGN, 24)(LEFT_ALIGN, 20);

                printer << matrix::core::init << "num" << "task_id" << "time" << "task_status" << matrix::core::endl;
                int i = 1;
                for(auto it = task_status_list.begin(); it != task_status_list.end(); ++it, ++i)
                {
                    printer << matrix::core::init << i << it->task_id << time_util::time_2_str(
                            it->create_time) << to_training_task_status_string(it->status) << matrix::core::endl;
                }
            }
        };

        class cmd_get_peer_nodes_resp_formater:public outputter
        {

        public:
            cmd_get_peer_nodes_resp_formater(std::shared_ptr<cmd_get_peer_nodes_resp> data)
            {
                m_data = data;
            };

            void format_output(matrix::service_core::get_peers_flag flag)
            {
                auto v = m_data.get();
                if(v == nullptr)
                {
                    return;
                }

                console_printer printer;
                if(matrix::service_core::flag_active == flag)
                {
                    //printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);

                    //printer << matrix::core::init << "peer_id" <<  "service_list" << matrix::core::endl;

                    printer(LEFT_ALIGN, 32)(LEFT_ALIGN, 10)(LEFT_ALIGN, 10)(LEFT_ALIGN, 48);

                    printer << matrix::core::init << "ip" << "port" << "nt" << "peer_id" << matrix::core::endl;

                }
                else if(matrix::service_core::flag_global == flag)
                {
                    printer(LEFT_ALIGN, 32)(LEFT_ALIGN, 10)(LEFT_ALIGN, 10)(LEFT_ALIGN, 30)(LEFT_ALIGN, 48);

                    printer << matrix::core::init << "ip" << "port" << "nt" << "status" << "peer_id" << matrix::core::endl;
                }

                auto it = v->peer_nodes_list.begin();
                for(; it != v->peer_nodes_list.end(); ++it)
                {
                    if(matrix::service_core::flag_active == flag)
                    {
                        //std::string svc_list;
                        //for (auto its = it->service_list.begin(); its != it->service_list.end(); ++its)
                        //{
                        //    svc_list += *its;
                        //    if (its + 1 != it->service_list.end())
                        //    {
                        //        svc_list += "|";
                        //    }
                        //}
                        printer << matrix::core::init << string_util::fuzz_ip(
                                it->addr.ip) << it->addr.port << (int32_t) it->node_type
                                //<< svc_list
                                << it->peer_node_id << matrix::core::endl;
                    }
                    else if(matrix::service_core::flag_global == flag)
                    {
                        printer << matrix::core::init << string_util::fuzz_ip(
                                it->addr.ip) << it->addr.port << (int32_t) it->node_type << matrix::service_core::net_state_2_string(
                                it->net_st) << (it->peer_node_id.empty()?"N/A":it->peer_node_id) << matrix::core::endl;
                    }
                }
            }

        private:
            std::shared_ptr<cmd_get_peer_nodes_resp> m_data;

        };

        class cmd_logs_req:public matrix::core::msg_base
        {
        public:

            std::string task_id;

            std::vector<std::string> peer_nodes_list;

            uint8_t head_or_tail;

            uint16_t number_of_lines;

            // for download training result
            std::string sub_op;
            std::string dest_folder;

        };

        class cmd_peer_node_log
        {
        public:

            std::string peer_node_id;

            std::string log_content;

        };

        //static std::string cmd_logs_resp::last_log_date="";

        class cmd_logs_resp:public matrix::core::msg_base, public outputter
        {
        public:

            int32_t result;
            std::string result_info;

            std::vector<cmd_peer_node_log> peer_node_logs;
            std::set<char> char_filter = {
                    /*0x00,0x01,0x02,*/0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                       0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,/*0x1b,*/0x1c,
                                       0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
                                       0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                                       0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
                                       0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
                                       0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
                                       0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
                                       0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
                                       0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f};

            // for download training result
            std::string sub_op;
            std::string dest_folder;

            // for log content check
            std::string task_id;

            // for duplicate log hiding
            // each log start with date info, like 2018-08-14T06:53:19.274586607Z
            enum
            {
                TIMESTAMP_STR_LENGTH = 30,
                IMAGE_HASH_STR_PREFIX_LENGTH = 12,
                IMAGE_HASH_STR_MAX_LENGTH = 64,
                MAX_NUM_IMAGE_HASH_LOG = 512

            };
            //bool no_duplicate;

            class series
            {
            public:
                std::string last_log_date;
                bool enable;
                std::set<std::string> image_download_logs;
            };

            static series m_series;

        public:

            void format_output();

            std::string get_training_result_hash_from_log(std::string& s);

        private:
            void format_output_segment();

            void format_output_series();

            void download_training_result();

            std::string get_log_date(std::string& s);


            bool is_image_download_str(std::string& s);

        };


        // sub operation of show cmd
        enum
        {
            OP_SHOW_NODE_INFO = 0, OP_SHOW_SERVICE_LIST = 1, OP_SHOW_UNKNOWN = 0xff
        };

        class cmd_show_req:public matrix::core::msg_base
        {
        public:
            std::string o_node_id;
            std::string d_node_id;
            std::vector<std::string> keys;
            int32_t op;
            std::string filter;
            std::string sort;

        public:
            cmd_show_req():
                    op(OP_SHOW_UNKNOWN), sort("gpu")
            {

            }

            void set_sort(std::string f)
            {

                const char* fields[] = {"gpu", "name", "version", "state", "timestamp", "service"};

                int n = sizeof(fields) / sizeof(char*);
                bool found = false;
                for(int i = 0; (i < n && !found); i++)
                {
                    if(f == std::string(fields[i]))
                    {
                        found = true;
                    }
                }

                sort = found?f:"gpu";
            }
        };

        class cmd_show_resp:public matrix::core::msg_base, public outputter
        {
        public:
            std::string o_node_id;
            std::string d_node_id;
            std::map<std::string, std::string> kvs;
            std::shared_ptr<std::map<std::string, node_service_info> > id_2_services;

            int32_t op;
            std::string err;

            std::string filter;
            std::string sort;


        public:

            cmd_show_resp();

            void error(std::string err_);

            std::string to_string(std::vector<std::string> in);

            void format_service_list();

            void format_node_info();

            void format_output();

            std::string to_time_str(time_t t);

        };

        /*class cmd_clear_req : public matrix::core::msg_base
        {
        public:

        };

        class cmd_clear_resp : public matrix::core::msg_base
        {
        public:

        };*/

        class cmd_ps_req:public matrix::core::msg_base
        {
        public:
            std::string task_id;
        };

        class cmd_ps_resp:public matrix::core::msg_base, public outputter
        {
        public:
            int32_t result;
            std::string result_info;

            std::vector<cmd_task_info> task_infos;

            void format_output()
            {
                if(E_SUCCESS != result)
                {
                    cout << result_info << endl;
                    return;
                }

                console_printer printer;
                printer(LEFT_ALIGN, 56)(LEFT_ALIGN, 24)(LEFT_ALIGN, 24)(LEFT_ALIGN, 20);

                printer << matrix::core::init << "task_id" << "time" << "task_status" << "result" << matrix::core::endl;
                for(cmd_task_info task_info : task_infos)
                {
                    printer << matrix::core::init << task_info.task_id << task_info.create_time << to_training_task_status_string(
                            task_info.status) << task_info.result << endl;
                }

            }

        };

        class api_call_handler
        {
        public:

            api_call_handler():
                    m_wait(std::make_shared<callback_wait<>>())
            {
//                init_subscription();
            }

            ~api_call_handler() = default;

            void init_subscription();

            template<typename req_type, typename resp_type>
            std::shared_ptr<resp_type> invoke(std::shared_ptr<req_type> req)
            {

                // m_mutex
                //This is a global lock that affects concurrent performance and will be optimized in the next release
                //
                std::unique_lock<std::mutex> lock(m_mutex);

                //construct message
                req->header.__set_session_id(id_generator().generate_session_id());
                std::shared_ptr<message> msg = std::make_shared<message>();
                msg->set_name(typeid(req_type).name());
                msg->set_content(req);

                m_wait->reset();

                //publish
                TOPIC_MANAGER->publish<int32_t>(msg->get_name(), msg);

                //synchronous wait for resp
                m_wait->wait_for(DEFAULT_CMD_LINE_WAIT_MILLI_SECONDS);
                if(true == m_wait->flag())
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

            std::mutex m_mutex;    //       currently no need.

            std::shared_ptr<matrix::core::base> m_resp;

            std::shared_ptr<callback_wait<>> m_wait;

        };

        extern std::unique_ptr<api_call_handler> g_api_call_handler;

    }

}
