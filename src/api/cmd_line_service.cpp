
#include <iostream>
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
            cin.ignore(std::numeric_limits<int>::max(), '\n');

            int m_argc = MAX_CMD_LINE_ARGS_COUNT;
            string_util::split(m_cmd_line_buf, ' ', m_argc, m_argv);
            //for (int i = 0; i < m_argc; i++)
            //{
            //    cout << m_argv[i] << endl;
            //}

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
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
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
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
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
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
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
                    ("task,t", bpo::value<std::string>(), "list a task");

                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                if (vm.count("all") || vm.count("a"))
                {
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
                }
                else if (vm.count("task") || vm.count("t"))
                {
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
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
                    //cout << vm["file"].as<std::string>() << endl;
                    //handler
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

    }

}