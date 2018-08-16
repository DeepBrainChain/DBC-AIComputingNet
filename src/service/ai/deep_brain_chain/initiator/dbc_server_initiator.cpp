/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   dbc_server_initiator.cpp
* description    :   dbc server initiator for dbc core
* date                  :   2018.01.20
* author             :   Bruce Feng
**********************************************************************************/
#include "dbc_server_initiator.h"
#if defined(__linux__) || defined(MAC_OSX)
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <stdio.h>
#endif
#include "server.h"
#include "dbc_conf_manager.h"
#include "connection_manager.h"
#include "env_manager.h"
#include "topic_manager.h"
#include "version.h"
#include "p2p_net_service.h"
#include "ai_power_requestor_service.h"
#include "ai_power_provider_service.h"
#include "cmd_line_service.h"
#include "common_service.h"
#include "id_generator.h"
#include "crypto_service.h"
#include "timer_matrix_manager.h"
#include "data_query_service.h"
#include <boost/exception/all.hpp>

using namespace std::chrono;
using namespace matrix::core;
using namespace matrix::service_core;
using namespace ai::dbc;


extern std::chrono::high_resolution_clock::time_point server_start_time;


namespace ai
{
    namespace dbc
    {

        int32_t dbc_server_initiator::init(int argc, char* argv[])
        {
            LOG_INFO << "begin to init dbc core";

            int32_t ret = E_SUCCESS;
            variables_map vm;
            std::shared_ptr<matrix::core::module> mdl(nullptr);

            //crypto service
            LOG_INFO << "begin to init crypto service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<crypto_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init crypto service successfully";

            //parse command line
            LOG_INFO << "begin to init command line";
            ret = parse_command_line(argc, argv, vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            LOG_INFO << "parse command line successfully";

            //env_manager
            LOG_INFO << "begin to init env manager";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<env_manager>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init env manager successfully";

            //core conf_manager
            LOG_INFO << "begin to init conf manager";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<conf_manager>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init conf manager successfully";

            //topic_manager
            LOG_INFO << "begin to init topic manager";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<topic_manager>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init topic manager successfully";

            //timer matrix manager
            LOG_INFO << "begin to init matrix manager";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<timer_matrix_manager>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init timer matrix manager successfully";

            //common service
            LOG_INFO << "begin to init common service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<common_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init common service successfully";

            //ai power requestor service
            LOG_INFO << "begin to init ai power requestor service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<ai_power_requestor_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init ai power requestor service successfully";

            //ai power provider service
            LOG_INFO << "begin to init ai power provider service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<ai_power_provider_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init ai power provider service successfully";

            //data query service
            LOG_INFO << "begin to init net misc service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<service::misc::data_query_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "init data query service but failed";
                return ret;
            }
            mdl->start();
            LOG_INFO << "init data query service successfully";


            //connection_manager
            LOG_INFO << "begin to init connection manager";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<connection_manager>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init connection manager successfully";

            //p2p net service
            LOG_INFO << "begin to init p2p net service";
            mdl = std::dynamic_pointer_cast<module>(std::make_shared<matrix::service_core::p2p_net_service>());
            g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
            ret = mdl->init(vm);
            if (E_SUCCESS != ret)
            {
                //logging
                return ret;
            }
            mdl->start();
            LOG_INFO << "init p2p net service successfully";

            //cmd line service
            if (false == m_daemon)
            {
                LOG_INFO << "begin to init command line service";
                mdl = std::dynamic_pointer_cast<module>(std::make_shared<cmd_line_service>());
                g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
                ret = mdl->init(vm);
                if (E_SUCCESS != ret)
                {
                    LOG_ERROR << "init command line service error.error code:" << ret;
                    return ret;
                }
                mdl->start();
                LOG_INFO << "init command line service successfully";
            }

            //log cost time
            high_resolution_clock::time_point init_end_time = high_resolution_clock::now();
            auto time_span_ms = duration_cast<milliseconds>(init_end_time - server_start_time);
            LOG_INFO << "init dbc core successfully, cost time: " << time_span_ms.count() << " ms";

            return E_SUCCESS;
        }

        int32_t dbc_server_initiator::exit()
        {
            return E_SUCCESS;
        }

        //parse command line
        int32_t dbc_server_initiator::parse_command_line(int argc, const char* const argv[], boost::program_options::variables_map &vm)
        {
            options_description opts("dbc command options");
            opts.add_options()
                ("help,h", "get dbc core help info")
                ("version,v", "get core version info")
                ("init,i", "init node id")
                ("daemon,d", "run as daemon process on Linux or Mac os")
                ("peer", bpo::value<std::vector<std::string>>(), "")
                ("ai_training,a", "run as ai training service provider")
                ("name,n", bpo::value<std::string>(), "node name")
                ("max_connect", bpo::value<int32_t>(), "")
                ("path", bpo::value<std::string>(), "path of dbc exe file")
                ("id", "get local node id");

            try
            {
                //parse
                bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
                bpo::notify(vm);

                //help
                if (vm.count("help") || vm.count("h"))
                {
                    cout << opts;
                    return E_EXIT_PARSE_COMMAND_LINE;
                }
                //version
                else if (vm.count("version") || vm.count("v"))
                {
                    std::string ver = STR_VER(CORE_VERSION);
                    cout << ver.substr(2, 2) << "." << ver.substr(4, 2) << "." << ver.substr(6, 2) << "." << ver.substr(8, 2);
                    cout << "\n";
                    return E_EXIT_PARSE_COMMAND_LINE;
                }
                else if (vm.count("init"))
                {
                    return on_cmd_init();
                }
                else if (vm.count("daemon") || vm.count("d"))
                {
                    return on_daemon();
                }
                else if (vm.count("id"))
                {
                    bpo::variables_map vm;

                    std::shared_ptr<matrix::core::module> mdl(nullptr);
                    mdl = std::dynamic_pointer_cast<module>(std::make_shared<env_manager>());
                    g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
                    auto ret = mdl->init(vm);
                    if (E_SUCCESS != ret)
                    {
                        return ret;
                    }

                    mdl = std::dynamic_pointer_cast<module>(std::make_shared<conf_manager>());
                    g_server->get_module_manager()->add_module(mdl->module_name(), mdl);
                    ret = mdl->init(vm);
                    if (E_SUCCESS != ret)
                    {
                        return ret;
                    }

                    cout << CONF_MANAGER->get_node_id();
                    cout << "\n";
                    return E_EXIT_PARSE_COMMAND_LINE;
                }
                //ignore
                else
                {
                    return E_SUCCESS;
                }
            }
            catch (const std::exception &e)
            {
                cout << "invalid command option " << e.what() << endl;
                cout << opts;
            }
            catch (...)
            {
                cout << argv[0] << " invalid command option" << endl;
                cout << opts;
            }

            return E_BAD_PARAM;
        }

        int32_t dbc_server_initiator::on_cmd_init()
        {
            //node info
            node_info info;
            id_generator gen;
            int32_t ret = gen.generate_node_info(info);                 //check: if exists, not init again and print prompt.
            if (E_SUCCESS != ret)
            {
                cout << "dbc init node info error" << endl;
                LOG_ERROR << "dbc_server_initiator init node info error";
                return ret;
            }

            //serialization
            ret = conf_manager::serialize_node_info(info);
            if (E_SUCCESS != ret)
            {
                cout << "dbc node info serialization failed." << endl;
                LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
                return ret;
            }
            cout << "node id: " << info.node_id << endl;
            LOG_DEBUG << "dbc_server_initiator init node info successfully, node_id: " << info.node_id;

            return E_EXIT_PARSE_COMMAND_LINE;
        }

        int32_t dbc_server_initiator::on_daemon()
        {
#if defined(__linux__) || defined(MAC_OSX)
            if (daemon(1, 1))               //log fd is reserved
            {
                LOG_ERROR << "dbc daemon error: " << strerror(errno);
                return E_DEFAULT;
            }

            LOG_DEBUG << "dbc daemon running succefully";

            //redirect std io to /dev/null
            int fd = open("/dev/null", O_RDWR);
            if (fd < 0)
            {
                LOG_ERROR << "dbc daemon open /dev/null error";
                return E_DEFAULT;
            }

            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);

            close(fd);

            m_daemon = true;
            return E_SUCCESS;
#else
            LOG_ERROR << "dbc daemon error:  not support";
            return E_DEFAULT;
#endif
        }

    }
}





