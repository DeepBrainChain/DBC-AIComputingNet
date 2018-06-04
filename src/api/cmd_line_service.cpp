/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : cmd_line_service.cpp
* description     : cmd line api service
* date                   : 2018.03.25
* author              : Bruce Feng
**********************************************************************************/
#include <iostream>
#include <limits>
#include <cstdint>
#include "cmd_line_service.h"
#include "util.h"
#include "server.h"



void cmd_line_task()
{
   static ai::dbc::cmd_line_service *service = (ai::dbc::cmd_line_service *)(g_server->get_module_manager()->get(CMD_LINE_API_MODULE).get());
    if (nullptr != service)
    {        
        service->on_usr_cmd();
    }    
}


using namespace std;
using namespace matrix::core;

namespace ai
{
    namespace dbc
    {
        static void print_cmd_usage()
        {
            cout << "we support commands: " << endl;
            cout << "help:          print out usage information" << endl;
            cout << "start:         start training" << endl;
            cout << "stop:          stop training" << endl;
            cout << "start_multi:   start multi training tasks" << endl;
            cout << "list:          list training tasks" << endl;
            cout << "peers:         get information of peers" << endl;
            cout << "logs:          get logs of task" << endl;
            cout << "quit / exit:   exit program" << endl;
            cout << "-----------------------------------------" << endl;
        }

        cmd_line_service::cmd_line_service() : m_argc(0)
        {
            memset(m_argv, 0x00, sizeof(m_argv));
            memset(m_cmd_line_buf, 0x00, sizeof(m_cmd_line_buf));

            init_cmd_invoker();
        }

        int32_t cmd_line_service::init(bpo::variables_map &options)
        { 
            cout << "Welcome to DeepBrain Chain AI world!" << std::endl;
            cout << std::endl;

            g_server->bind_idle_task(&cmd_line_task);

            return E_SUCCESS;
        }

        void cmd_line_service::init_cmd_invoker()
        {
            m_invokers["start"] = std::bind(&cmd_line_service::start_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["stop"] = std::bind(&cmd_line_service::stop_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["start_multi"] = std::bind(&cmd_line_service::start_multi_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["list"] = std::bind(&cmd_line_service::list_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["peers"] = std::bind(&cmd_line_service::get_peers, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["logs"] = std::bind(&cmd_line_service::logs, this, std::placeholders::_1, std::placeholders::_2);
        }
        
        void cmd_line_service::on_usr_cmd()
        {
            cout << "dbc>>>";

            //read user input
            cin.get(m_cmd_line_buf, MAX_CMD_LINE_BUF_LEN);
            cin.clear();
            cin.ignore((std::numeric_limits<int>::max)(), '\n');

            int m_argc = MAX_CMD_LINE_ARGS_COUNT;
            memset(m_argv, 0x00, sizeof(m_argv));

            //split parse
            string_util::split(m_cmd_line_buf, ' ', m_argc, m_argv);
            if (0 == m_argc)
            {
                return;
            }
            if (std::string(m_argv[0]).compare("help") == 0)
            {
                print_cmd_usage();
                return;
            }
            else if (std::string(m_argv[0]).compare("quit") == 0 || std::string(m_argv[0]).compare("exit") == 0)
            {
                g_server->set_exited(true);
                return;
            }

            auto it = m_invokers.find(m_argv[0]);
            if (it == m_invokers.end())
            {
                cout << "unknown command, for prompt please input 'help'" << endl;
                return;
            }

            //call handler function
            auto func = it->second;
            func(m_argc, m_argv);
        }

        void cmd_line_service::start_training(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("start task options");

            try
            {
                opts.add_options()
                    ("help,h", "start task")
                    ("config,c", bpo::value<std::string>(), "task config file path");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("config") || vm.count("c"))
                {
                    std::shared_ptr<cmd_start_training_req> req = std::make_shared<cmd_start_training_req>();
                    req->task_file_path = vm["config"].as<std::string>();

                    std::shared_ptr<cmd_start_training_resp> resp = m_handler.invoke<cmd_start_training_req, cmd_start_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }
        
        void cmd_line_service::stop_training(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("stop task options");

            try
            {
                opts.add_options()
                    ("help,h", "stop task")
                    ("task,t", bpo::value<std::string>(), "task id");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("task") || vm.count("t"))
                {
                    std::shared_ptr<cmd_stop_training_req> req = std::make_shared<cmd_stop_training_req>();
                    req->task_id = vm["task"].as<std::string>();

                    std::shared_ptr<cmd_stop_training_resp> resp = m_handler.invoke<cmd_stop_training_req, cmd_stop_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }
        
        void cmd_line_service::start_multi_training(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("start multi task options");

            try
            {
                opts.add_options()
                    ("help,h", "start multi task")
                    ("config,c", bpo::value<std::string>(), "task config file path");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("config") || vm.count("c"))
                {
                    std::shared_ptr<cmd_start_multi_training_req> req = std::make_shared<cmd_start_multi_training_req>();
                    req->mulit_task_file_path = vm["config"].as<std::string>();

                    std::shared_ptr<cmd_start_multi_training_resp> resp = m_handler.invoke<cmd_start_multi_training_req, cmd_start_multi_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }
        
        void cmd_line_service::list_training(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("list task options");

            try
            {
                opts.add_options()
                    ("help,h", "list task")
                    ("all,a", "list all task")
                    ("task,t", bpo::value<std::vector<std::string>>(), "list a task");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("all") || vm.count("a"))
                {
                    std::shared_ptr<cmd_list_training_req> req= std::make_shared<cmd_list_training_req>();
                    req->list_type = LIST_ALL_TASKS;

                    std::shared_ptr<cmd_list_training_resp> resp = m_handler.invoke<cmd_list_training_req, cmd_list_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("task") || vm.count("t"))
                {
                    std::shared_ptr<cmd_list_training_req> req(new cmd_list_training_req);
                    req->list_type = LIST_SPECIFIC_TASKS;
                    std::vector<std::string> task_vector = vm["task"].as<std::vector<std::string>>();
                    std::copy(std::make_move_iterator(task_vector.begin()), std::make_move_iterator(task_vector.end()), std::back_inserter(req->task_list));
                    task_vector.clear();

                    std::shared_ptr<cmd_list_training_resp> resp = m_handler.invoke<cmd_list_training_req, cmd_list_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }
        
        void cmd_line_service::get_peers(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("list peers options");

            try
            {
                opts.add_options()
                    ("help,h", "list peers")
                    ("active,a", "list active peers")
                    ("global,g", "list global peers");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("active") || vm.count("a") || vm.count("global") || vm.count("g"))
                {
                    std::shared_ptr<cmd_get_peer_nodes_req> req = std::make_shared<cmd_get_peer_nodes_req>();
                    if (vm.count("active") || vm.count("a"))
                    {
                        req->flag = ::dbc::flag_active;
                    }
                    else if (vm.count("global") || vm.count("g"))
                    {
                        req->flag = ::dbc::flag_global;
                    }

                    std::shared_ptr<cmd_get_peer_nodes_resp> resp = m_handler.invoke<cmd_get_peer_nodes_req, cmd_get_peer_nodes_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        cmd_get_peer_nodes_resp_formater formater(resp);
                        formater.format_output();
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }

        void cmd_line_service::logs(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("task logs options");

            try
            {
                opts.add_options()
                    ("help,h", "get task running logs")
                    ("tail", bpo::value<std::string>(), "get log from tail")
                    ("head", bpo::value<std::string>(), "get log from head")
                    ("task,t", bpo::value<std::string>(), "task id");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("task") || vm.count("t"))
                {
                    std::shared_ptr<cmd_logs_req> req = std::make_shared<cmd_logs_req>();
                    req->task_id = vm["task"].as<std::string>();

                    std::string number_of_lines;

                    //tail or head
                    if (vm.count("tail"))
                    {
                        req->head_or_tail = GET_LOG_TAIL;          //get log from tail
                        number_of_lines = vm["tail"].as<std::string>();
                    }
                    else if (vm.count("head"))
                    {
                        req->head_or_tail = GET_LOG_HEAD;          //get log from head
                        number_of_lines = vm["head"].as<std::string>();
                    }
                    else
                    {
                        req->head_or_tail = GET_LOG_TAIL;          //get log from tail
                    }

                    //number of lines
                    if (number_of_lines == "all")
                    {
                        req->number_of_lines = 0;
                    }
                    else if (number_of_lines == DEFAULT_STRING)
                    {
                        req->number_of_lines = DEFAULT_NUMBER_OF_LINES;
                    }
                    else
                    {
                        uint16_t lines = (uint16_t) std::stoul(number_of_lines);
                        req->number_of_lines = lines > MAX_NUMBER_OF_LINES ? MAX_NUMBER_OF_LINES : lines;
                    }

                    std::shared_ptr<cmd_logs_resp> resp = m_handler.invoke<cmd_logs_req, cmd_logs_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        format_output(resp);
                    }
                }
                else if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                }
                else
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                }

            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }

        template<typename resp_type>
        void cmd_line_service::format_output(std::shared_ptr<resp_type> resp)
        {
            if (nullptr == resp)
            {
                return;
            }

            //format output
            resp->format_output();
        }

    }

}