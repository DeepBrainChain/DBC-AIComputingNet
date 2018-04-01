
#include <iostream>
#include <limits>
#include <cstdint>
#include "cmd_line_service.h"
#include "util.h"
#include "server.h"
#include "console_printer.h"


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

        cmd_line_service::cmd_line_service() : m_argc(0)
        {
            memset(m_argv, 0x00, sizeof(m_argv));
            memset(m_cmd_line_buf, 0x00, sizeof(m_cmd_line_buf));

            m_invokers["start"] = std::bind(&cmd_line_service::start_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["stop"] = std::bind(&cmd_line_service::stop_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["start_multi"] = std::bind(&cmd_line_service::start_multi_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["list"] = std::bind(&cmd_line_service::list_training, this, std::placeholders::_1, std::placeholders::_2);
            m_invokers["peers"] = std::bind(&cmd_line_service::get_peers, this, std::placeholders::_1, std::placeholders::_2);
        }

        int32_t cmd_line_service::init(bpo::variables_map &options) 
        { 
            g_server->bind_idle_task(&cmd_line_task);

            return E_SUCCESS;
        }

        void cmd_line_service::on_usr_cmd()
        {
            cout << "dbc>>>";

            //read user input
            cin.get(m_cmd_line_buf, MAX_CMD_LINE_BUF_LEN);
            cin.clear();
            cin.ignore((std::numeric_limits<int>::max)(), '\n');

            int m_argc = MAX_CMD_LINE_ARGS_COUNT;
            string_util::split(m_cmd_line_buf, ' ', m_argc, m_argv);

            auto it = m_invokers.find(m_argv[0]);
            if (it == m_invokers.end())
            {
                cout << "unknown command " << endl;
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
                    std::shared_ptr<cmd_start_training_req> req(new cmd_start_training_req);
                    req->task_file_path = vm["config"].as<std::string>();

                    std::shared_ptr<cmd_start_training_resp> resp = m_handler.invoke<cmd_start_training_req, cmd_start_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << "command time out" << endl;
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
                    std::shared_ptr<cmd_stop_training_req> req(new cmd_stop_training_req);
                    req->task_id = vm["task"].as<std::string>();

                    std::shared_ptr<cmd_stop_training_resp> resp = m_handler.invoke<cmd_stop_training_req, cmd_stop_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << "command time out" << endl;
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
                    std::shared_ptr<cmd_start_multi_training_req> req(new cmd_start_multi_training_req);
                    req->mulit_task_file_path = vm["config"].as<std::string>();

                    std::shared_ptr<cmd_start_multi_training_resp> resp = m_handler.invoke<cmd_start_multi_training_req, cmd_start_multi_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << "command time out" << endl;
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
                    std::shared_ptr<cmd_list_training_req> req(new cmd_list_training_req);
                    req->list_type = LIST_ALL_TASKS;

                    std::shared_ptr<cmd_list_training_resp> resp = m_handler.invoke<cmd_list_training_req, cmd_list_training_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << "command time out" << endl;
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
                        cout << "command time out" << endl;
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
                    ("all,a", "list all peers");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("all") || vm.count("a"))
                {
                    std::shared_ptr<cmd_get_peer_nodes_req> req(new cmd_get_peer_nodes_req);

                    std::shared_ptr<cmd_get_peer_nodes_resp> resp = m_handler.invoke<cmd_get_peer_nodes_req, cmd_get_peer_nodes_resp>(req);
                    if (nullptr == resp)
                    {
                        cout << "command time out" << endl;
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

        void cmd_line_service::format_output(std::shared_ptr<cmd_start_training_resp> resp)
        {
            if (E_SUCCESS != resp->result)
            {
                cout << resp->result_info << endl;
            }

            cout << resp->task_id << endl;
        }

        void cmd_line_service::format_output(std::shared_ptr<cmd_stop_training_resp> resp)
        {
            if (E_SUCCESS != resp->result)
            {
                cout << resp->result_info << endl;
            }

            cout << "stopping training task......" << endl;
        }

        void cmd_line_service::format_output(std::shared_ptr<cmd_list_training_resp> resp)
        {
            console_printer printer;            
            printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 10);

            printer << matrix::core::init << "task_id" << "task_status" << matrix::core::endl;

            auto it = resp->task_status_list.begin();
            for (; it != resp->task_status_list.end(); it++)
            {
                printer << matrix::core::init << it->task_id << it->status << matrix::core::endl;
            }
        }

        void cmd_line_service::format_output(std::shared_ptr<cmd_start_multi_training_resp> resp)
        {
            cout << "task_id" << endl;

            auto it = resp->task_list.begin();
            for ( ; it != resp->task_list.end(); it++)
            {
                cout << *it << endl;
            }
        }

        void cmd_line_service::format_output(std::shared_ptr<cmd_get_peer_nodes_resp> resp)
        {
            console_printer printer;
            printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 64)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);

            printer << matrix::core::init << "peer_id" << "time_stamp" << "ip" << "port" << "service_list" << matrix::core::endl;

            auto it = resp->peer_nodes_list.begin();
            for (; it != resp->peer_nodes_list.end(); it++)
            {
                std::string service_list;  //left to later
                printer << matrix::core::init << it->peer_node_id << it->live_time_stamp << it->addr.ip << it->addr.port << service_list << matrix::core::endl;
            }
        }

    }

}