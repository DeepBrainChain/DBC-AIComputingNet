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
//#include <unistd.h>
#include "cmd_line_service.h"
#include "util.h"
#include "server.h"
#ifndef WIN32
#include "readline/readline.h"
#include "readline/history.h"
#endif

#include "log.h"
#include "filter/simple_expression.h"

static void cmd_line_task()
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
            cout << "show:          get attributes from taget node" <<endl;
            cout << "ps:            print ai request task cache info" << endl;
            cout << "clear:         clean screen" << endl;
            cout << "quit / exit:   exit program" << endl;
            cout << "-----------------------------------------" << endl;
        }

        cmd_line_service::cmd_line_service()
        {
            m_argvs.clear();
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
            m_invokers["show"] = std::bind(&cmd_line_service::show, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["clear"] = std::bind(&cmd_line_service::clear, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["ps"] = std::bind(&cmd_line_service::ps, this, std::placeholders::_1, std::placeholders::_2);
        }
        
        void cmd_line_service::on_usr_cmd()
        {
#ifdef WIN32
            cout << "dbc>>> ";

            //read user input
            cin.get(m_cmd_line_buf, MAX_CMD_LINE_BUF_LEN);
            cin.clear();
            cin.ignore((std::numeric_limits<int>::max)(), '\n');
#else
            try
            {
                auto line = readline("dbc>>> ");
                if (line == nullptr)
                {
                    LOG_ERROR << "readline return nullptr";
                    return;
                }

                if (line[0] != 0)
                {
                    // save the cmd if it is not empty
                    add_history(line);
                }

                strncpy(m_cmd_line_buf, line, MAX_CMD_LINE_BUF_LEN);
                m_cmd_line_buf[MAX_CMD_LINE_BUF_LEN - 1] = 0;

                free(line);
            }
            catch (const std::exception &e)
            {
                std::cout << e.what();
                return;
            }
            
#endif

#ifdef WIN32
            m_argvs = bpo::split_winmain(m_cmd_line_buf);
#else
            m_argvs = bpo::split_unix(m_cmd_line_buf);
#endif

            if (0 == m_argvs.size())
            {
                return;
            }

            std::string cmd = m_argvs[0];
            if (cmd.compare("help") == 0)
            {
                print_cmd_usage();
                return;
            }
            else if (cmd.compare("quit") == 0 || cmd.compare("exit") == 0)
            {
                g_server->set_exited(true);
                return;
            }

            auto it = m_invokers.find(cmd);
            if (it == m_invokers.end())
            {
                cout << "unknown command, for prompt please input 'help'" << endl;
                return;
            }

            //call handler function
            auto func = it->second;
            char ** argv = new char* [m_argvs.size()];
            for (uint32_t i=0; i<m_argvs.size(); i++ )
            {
                argv[i] = (char *)m_argvs[i].c_str();
            }
            func(m_argvs.size(), argv);
            delete []argv;
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
                        req->flag = matrix::service_core::flag_active;
                    }
                    else if (vm.count("global") || vm.count("g"))
                    {
                        req->flag = matrix::service_core::flag_global;
                    }

                    std::shared_ptr<cmd_get_peer_nodes_resp> resp = m_handler.invoke<cmd_get_peer_nodes_req, cmd_get_peer_nodes_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << endl << "command time out" << endl;
                    }
                    else
                    {
                        cmd_get_peer_nodes_resp_formater formater(resp);
                        formater.format_output(req->flag);
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
                        uint64_t lines = (uint64_t) std::stoull(number_of_lines);
                        //req->number_of_lines = lines > MAX_NUMBER_OF_LINES ? MAX_NUMBER_OF_LINES : lines;
                        if (lines > MAX_NUMBER_OF_LINES)
                        {
                            cout << argv[0] << " tail number_of_lines must < " << MAX_NUMBER_OF_LINES << endl;
                            cout << opts;
                            return;
                        }

                        req->number_of_lines = (uint16_t)lines;
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

        void cmd_line_service::clear(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("clear info options");
            try
            {
                opts.add_options()
                        ("help,h", "clear help")
                        //("all,a", "clear all info in this node")
                        ("screen,s", "clear screen");
                std::string cmd = "";
#if  defined(__linux__) || defined(MAC_OSX)
                cmd = "clear";
#elif defined(WIN32)
                cmd = "cls";

#endif
                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                    return;
                }

                if (vm.count("screen") || vm.count("s"))
                {
                    system(cmd.c_str());
                    return;
                }

               /* if (vm.count("all") || vm.count("a"))
                {
                    std::shared_ptr<cmd_clear_req> req = std::make_shared<cmd_clear_req>();
                    std::shared_ptr<cmd_clear_resp> resp = m_handler.invoke<cmd_clear_req, cmd_clear_resp>(req);
                    system("cls");
                    return;
                }*/

                system(cmd.c_str());

            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }

        void cmd_line_service::ps(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("clear info options");
            try
            {
                opts.add_options()
                    ("help,h", "clear help")
                    ("all,a","show all task cache state")
                    ("task,t",bpo::value<std::string>(), "show assign task cache state");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                    return;
                }

                if (vm.count("task") || vm.count("t"))
                {
                    std::shared_ptr<cmd_ps_req> req = std::make_shared<cmd_ps_req>();
                    req->task_id = vm["task"].as<std::string>();;
                    std::shared_ptr<cmd_ps_resp> resp = m_handler.invoke<cmd_ps_req, cmd_ps_resp>(req);
                    format_output(resp);
                    return;
                }

                if (vm.count("all") || vm.count("a"))
                {
                    std::shared_ptr<cmd_ps_req> req = std::make_shared<cmd_ps_req>();
                    req->task_id = "all";;
                    std::shared_ptr<cmd_ps_resp> resp = m_handler.invoke<cmd_ps_req, cmd_ps_resp>(req);
                    format_output(resp);
                    return;
                }
            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }

        void cmd_line_service::show(int argc, char* argv[])
        {
            bpo::variables_map vm;
            options_description opts("show node info options");

            try
            {
                opts.add_options()
                        ("help,h", "show service info and specific node info")
                        ("node,n", bpo::value<std::string>(), "print node's hardware info of indicated node id")
                        ("keys,k", bpo::value<std::vector<std::string>>(), "if -n is specified, only print the value of indicated attribute.")
                        ("service,s", "print nodes' service info in the network")
                        ("filter,f", bpo::value<std::vector<std::string>>(), "if -s is specified, only print node matchs the filter.");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                std::shared_ptr<cmd_show_req> req= std::make_shared<cmd_show_req>();

                if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                    return;
                }

                if (vm.count("node") || vm.count("n"))
                {
                    req->o_node_id = CONF_MANAGER->get_node_id();
                    req->d_node_id = vm["node"].as<std::string>();
                    req->op = OP_SHOW_NODE_INFO;


                    if (vm.count("keys") || vm.count("k"))
                    {
                        std::vector<std::string> tmp = vm["keys"].as<std::vector<std::string>>();
                        std::copy(std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()), std::back_inserter(req->keys));
                        tmp.clear();
                    }
                    else
                    {
                        // default key: all
                        req->keys.push_back("all");
                    }
                }

                if (vm.count("service") || vm.count("s"))
                {
                    req->op = OP_SHOW_SERVICE_LIST;


                    if (vm.count("filter") || vm.count("f"))
                    {
                        std::vector<std::string> tmp = vm["filter"].as<std::vector<std::string>>();

                        req->filter = "";
                        for (auto &s: tmp)
                        {
                            if (req->filter.length() != 0)
                            {
                                req->filter += (" and " + s);
                            }
                            else
                            {
                                req->filter += s;
                            }
                        }

                        expression e(req->filter);
                        if (!e.is_valid())
                        {
                            cout << endl << "invalid filter " << req->filter << endl;
                            return;
                        }
                    }
                }

                if(OP_SHOW_UNKNOWN == req->op)
                {
                    cout << argv[0] << " invalid option" << endl;
                    cout << opts;
                    return;
                }

                std::shared_ptr<cmd_show_resp> resp = m_handler.invoke<cmd_show_req, cmd_show_resp>(req);
                if (nullptr == resp)
                {
                    cout << endl << "command time out" << endl;
                    return;
                }
                else
                {
                    LOG_DEBUG << "cmd_show_resp received!";
                    format_output(resp);
                }

            }
            catch (...)
            {
                cout << argv[0] << " invalid option" << endl;
                cout << opts;
            }
        }

    }

}
