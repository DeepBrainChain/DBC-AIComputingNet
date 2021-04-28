/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        conf_manager.cpp
* description    conf manager for core
* date                  : 2017.08.16
* author            Bruce Feng
**********************************************************************************/
#include "conf_manager.h"
#include "env_manager.h"
#include <fstream> 
#include <boost/exception/all.hpp>
#include <vector>
#include "common/util.h"
#include "utilstrencodings.h"
#include <boost/format.hpp>

std::string DEFAULT_VM_LISTEN_PORT("16509");
std::string DEFAULT_CONTAINER_LISTEN_PORT("31107");
std::string DEFAULT_REST_PORT("41107");
std::string DEFAULT_CONTAINER_IMAGE_NAME("dbctraining/tensorflow-cpu-0.1.0:v1");
const int32_t DEFAULT_MAX_CONNECTION_NUM = 128;
const int32_t DEFAULT_TIMER_DBC_REQUEST_IN_SECOND = 20;
const int32_t DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND = 30;
const int32_t DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND = 300;
const int32_t DEFAULT_SPEED = 0;
const bool DEFAULT_ENABLE = true;
const bool DEFAULT_DISABLE = false;
const int32_t DEFAULT_UPDATE_IDLE_TASK_CYCLE = 24*60;   //24h
const int32_t DEFAULT_TIMER_AI_TRAINING_TASK_SCHEDULE_IN_SECOND = 15;
const int32_t DEFAULT_TIMER_LOG_REFRESH_IN_SECOND = 5;
const int64_t DEFAULT_USE_SIGN_TIME = 0x7FFFFFFFFFFFFFFF;
const int16_t DEFAULT_PRUNE_CONTAINER_INTERVAL=240; //48 hours,test:1 hours
const int16_t DEFAULT_PRUNE_TASK_INTERVAL=2400;     //100 days
const int16_t DEFALUT_PRUNE_DOCKER_ROOT_USE_RATIO=5;


namespace matrix
{
    namespace core
    {

        conf_manager::conf_manager() 
            : m_log_level(1)
            , m_net_params(std::make_shared<net_type_params>())
        {
            m_proto_capacity.add(matrix_capacity::THRIFT_BINARY_C_NAME);
            m_proto_capacity.add(matrix_capacity::THRIFT_COMPACT_C_NAME);
            m_proto_capacity.add(matrix_capacity::SNAPPY_RAW_C_NAME);
        }

        int32_t conf_manager::init(bpo::variables_map &options)
        {
            int32_t ret = E_SUCCESS;
            
            //parse local conf
            ret = parse_local_conf();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "conf manager parse local conf error and exit";
                return ret;
            }

            //parse node.dat
            ret = parse_node_dat();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "conf manager parse node dat error and exit";
                return ret;
            }

            //init params
            ret = init_params();
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "conf manager init params error and exit";
                return ret;
            }

            //update log level
            log::set_filter_level((boost::log::trivial::severity_level) m_log_level);

            return E_SUCCESS;
        }

        int32_t conf_manager::parse_local_conf()
        {
            ////modify by regulus:fix validate port error. use string instead ulong in the options.
            //core opt description
            bpo::options_description core_opts("config file options");
            core_opts.add_options()
                ("log_level", bpo::value<uint32_t>()->default_value(DEFAULT_LOG_LEVEL))
                ("net_type", bpo::value<std::string>()->default_value(DEFAULT_NET_TYPE), "")
                ("host_ip", bpo::value<std::string>()->default_value(DEFAULT_LOCAL_IP), "")
                ("main_net_listen_port", bpo::value<std::string>()->default_value(DEFAULT_MAIN_NET_LISTEN_PORT), "")
                ("test_net_listen_port", bpo::value<std::string>()->default_value(DEFAULT_TEST_NET_LISTEN_PORT), "")
                ("container_ip", bpo::value<std::string>()->default_value(DEFAULT_LOCAL_IP), "")
                ("container_port", bpo::value<std::string>()->default_value(DEFAULT_CONTAINER_LISTEN_PORT), "")
                ("max_connect", bpo::value<int32_t>()->default_value(128), "")
                ("timer_service_broadcast_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND), "")
                ("timer_dbc_request_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_DBC_REQUEST_IN_SECOND), "")
                ("timer_ai_training_task_schedule_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_AI_TRAINING_TASK_SCHEDULE_IN_SECOND), "")
                ("timer_log_refresh_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_LOG_REFRESH_IN_SECOND), "")
                ("magic_num", bpo::value<std::string>()->default_value("0XE1D1A098"), "")
                ("oss_url", bpo::value<std::string>()->default_value(""), "")
                ("oss_crt", bpo::value<std::string>()->default_value(""), "")
                ("max_recv_speed", bpo::value<int32_t>()->default_value(0), "")
                ("enable_idle_task", bpo::value<bool>()->default_value(true), "")
                ("enable_node_reboot", bpo::value<bool>()->default_value(false), "")
                ("enable_billing", bpo::value<bool>()->default_value(true), "")
                ("update_idle_task_cycle", bpo::value<int32_t>()->default_value(DEFAULT_UPDATE_IDLE_TASK_CYCLE), "")
                ("timer_service_list_expired_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND), "")
                ("rest_ip", bpo::value<std::string>()->default_value(DEFAULT_LOOPBACK_IP), "http server ip address")
                ("use_sign_time", bpo::value<int64_t>()->default_value(DEFAULT_USE_SIGN_TIME), "")
                ("prune_container_stop_interval", bpo::value<int16_t>()->default_value(DEFAULT_PRUNE_CONTAINER_INTERVAL), "")
                ("prune_docker_root_use_ratio", bpo::value<int16_t>()->default_value(DEFALUT_PRUNE_DOCKER_ROOT_USE_RATIO), "")
                ("prune_task_stop_interval", bpo::value<int16_t>()->default_value(DEFAULT_PRUNE_TASK_INTERVAL), "")
                ("rest_port", bpo::value<std::string>()->default_value(DEFAULT_REST_PORT), "0 prohibit http server")
                ("auth_mode", bpo::value<std::string>()->default_value(""), "")
                ("trust_node_id", bpo::value<std::vector<std::string>>(), "")
                ("wallet", bpo::value<std::vector<std::string>>(), "");

            //peer opt description
            bpo::options_description peer_opts("peer options");
            peer_opts.add_options()
                ("peer", bpo::value<std::vector<std::string>>(), "");

            //file path
            const fs::path &conf_path = env_manager::get_conf_path();
            const fs::path &peer_path = env_manager::get_peer_path();
            try
            {
                //core.conf
                std::ifstream conf_ifs(conf_path.generic_string());
                bpo::store(bpo::parse_config_file(conf_ifs, core_opts), m_args);
                
                //peer.conf
                std::ifstream peer_ifs(peer_path.generic_string());
                bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "conf manager parse local conf error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }


        bool conf_manager::check_node_info()
        {
            //id_generator gen;

            if (id_generator().check_node_id(m_node_id) != true)
            {
                return false;
            }
            std::string node_derived;
            bool derived = id_generator().derive_node_id_by_private_key(m_node_private_key, node_derived);
            if (derived != true)
            {
                return derived;
            }

            return node_derived == m_node_id;
        }

        int32_t conf_manager::gen_new_nodeid()
        {
            //node info
            node_info info;
            id_generator gen;
            int32_t ret = gen.generate_node_info(info);                 //check: if exists, not init again and print promption.
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "dbc_server_initiator init node info error";
                return ret;
            }

            //serialization
            ret = conf_manager::serialize_node_info(info);
            if (E_SUCCESS != ret)
            {
                LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
                return ret;
            }
            LOG_DEBUG << "dbc_server_initiator init node info successfully, node_id: " << info.node_id;
            return E_SUCCESS;
        }

        int32_t conf_manager::parse_node_dat()
        {
            //node dat description
            bpo::options_description node_dat_opts("node dat description");
            node_dat_opts.add_options()
                ("node_id", bpo::value<std::string>(), "")
                ("node_private_key", bpo::value<std::string>(), "");

            //node.dat path
            fs::path node_dat_path = env_manager::get_dat_path();
            node_dat_path /= fs::path(NODE_FILE_NAME);
            
            if (!fs::exists(node_dat_path) || fs::is_empty(node_dat_path))
            {
                //if not exist then gen new nodeid
                int32_t gen_ret = gen_new_nodeid();
                if (gen_ret != E_SUCCESS)
                {

                    LOG_ERROR << "generate new nodeid error";
                    return gen_ret;
                }
            }

            if (!fs::exists(node_dat_path) || fs::is_empty(node_dat_path))
            {
                //std::cout << "Parse node.dat error. Please try ./dbc --init command to init node id if node id not inited." << std::endl;
                LOG_ERROR << "Parse node.dat error. Please try dbc --init command to init node id if node id not inited." ;
                return E_DEFAULT;
            }


            try
            {
                std::ifstream node_dat_ifs(node_dat_path.generic_string());
                bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                //std::cout << "Parse node.dat error. Please try ./dbc --init command to init node id if node id not inited." << std::endl;
                LOG_ERROR << "Parse node.dat error. Please try dbc --init command to init node id if node id not inited." ;

                LOG_ERROR << "conf manager parse node.dat error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t conf_manager::init_params()
        {
            //node id
            if (0 == m_args.count("node_id"))
            {
                LOG_ERROR << "conf_manager has no node id error";
                return E_DEFAULT;
            }
            m_node_id = SanitizeString(m_args["node_id"].as<std::string>());

            //node private key
            if (0 == m_args.count("node_private_key"))
            {
                LOG_ERROR << "conf_manager has no node private key error";
                return E_DEFAULT;
            }
            m_node_private_key = SanitizeString(m_args["node_private_key"].as<std::string>());

            //net type
            if (0 == m_args.count("net_type"))
            {
                LOG_ERROR << "conf_manager has no node type error";
                return E_DEFAULT;
            }

            if (check_node_info() != true)
            {
                LOG_ERROR << "node_id error";
                return E_DEFAULT;
            }
            
            m_net_type = m_args["net_type"].as<std::string>();

            //net flag
            init_net_flag();

            assert(nullptr != m_net_params);

            //net port
            if (m_net_type == MAIN_NET_TYPE)
            {
                const std::string & net_listen_port = m_args.count("main_net_listen_port") ? m_args["main_net_listen_port"].as<std::string>() : DEFAULT_MAIN_NET_LISTEN_PORT;
                m_net_params->init_net_listen_port(net_listen_port);
            }
            else if (m_net_type == TEST_NET_TYPE)
            {
                const std::string & net_listen_port = m_args.count("test_net_listen_port") ? m_args["test_net_listen_port"].as<std::string>() : DEFAULT_TEST_NET_LISTEN_PORT;
                m_net_params->init_net_listen_port(net_listen_port);
            }
            else
            {
                LOG_ERROR << "net type not support yet";
                return E_DEFAULT;
            }

            //init seeds
            m_net_params->init_seeds();

            //init log level
            if (0 != m_args.count("log_level"))
            {
                m_log_level = m_args["log_level"].as<uint32_t>();
            }

            return E_SUCCESS;
        }

        void conf_manager::init_net_flag()
        {
            if (m_net_type == MAIN_NET_TYPE)
            {
                m_net_flag = MAIN_NET;
            }
            else if (m_net_type == TEST_NET_TYPE)
            {
                //m_net_flag = TEST_NET;
                std::string magic_num = m_args["magic_num"].as<std::string>();
                try
                {
                    m_net_flag = std::stoll(magic_num, 0, 0);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "magic_num abnormal." << m_args["magic_num"].as<std::string>() << ", " << e.what();
                    m_net_flag = TEST_NET; 
                    LOG_DEBUG << "use magic num:" << boost::str(boost::format("0x%0x") % m_net_flag);
                    return;
                }
            }
            else
            {
                m_net_flag = MAIN_NET;
            }
            LOG_DEBUG << "use magic num:" << boost::str(boost::format("0x%0x") % m_net_flag);
        }

        int32_t conf_manager::serialize_node_info(const node_info &info)
        {
            FILE *fp = nullptr;

            //node.dat path
            fs::path node_dat_path;

            try
            {
                node_dat_path /= matrix::core::path_util::get_exe_dir();
                node_dat_path /= fs::path(DAT_DIR_NAME);

                //check dat directory
                if (false == fs::exists(node_dat_path))
                {
                    LOG_DEBUG << "dat directory path does not exist and create dat directory";
                    fs::create_directory(node_dat_path);
                }

                //check dat directory
                if (false == fs::is_directory(node_dat_path))
                {
                    LOG_ERROR << "dat directory path does not exist and exit";
                    return E_DEFAULT;
                }

                node_dat_path /= fs::path(NODE_FILE_NAME);

                //open file w+
    #ifdef WIN32
                errno_t err = fopen_s(&fp, node_dat_path.generic_string().c_str(), "w+");
                if (0 != err)
    #else
                fp = fopen(node_dat_path.generic_string().c_str(), "w+");
                if (nullptr == fp)
    #endif
                {
                    LOG_ERROR << "conf_manager open node.dat error: fp is nullptr";
                    return E_DEFAULT;
                }
            }
            catch (const std::exception & e)
            {
                LOG_ERROR << "create node error: " << e.what();
                return E_DEFAULT;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "create node error" << diagnostic_information(e);
                return E_DEFAULT;
            }

            assert(nullptr != fp);

            fprintf(fp, "node_id=");
            fprintf(fp, "%s",info.node_id.c_str());
            fprintf(fp, "\n");

            fprintf(fp, "node_private_key=");
            fprintf(fp, "%s",info.node_private_key.c_str());
            fprintf(fp, "\n");

            if (fp)
            {
                fclose(fp);
            }
            return E_SUCCESS;
        }

        uint16_t conf_manager::get_net_default_port()
        {
            if (m_net_type == MAIN_NET_TYPE)
            {
                return (uint16_t)std::stoi(DEFAULT_MAIN_NET_LISTEN_PORT);
            }
            else if (m_net_type == TEST_NET_TYPE)
            {
                return (uint16_t)std::stoi(DEFAULT_TEST_NET_LISTEN_PORT);
            }
            else
            {
                return (uint16_t)std::stoi(DEFAULT_MAIN_NET_LISTEN_PORT);
            }
        }

    }

}
