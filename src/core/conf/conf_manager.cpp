/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºconf_manager.cpp
* description    £ºconf manager for core
* date                  : 2017.08.16
* author            £ºBruce Feng
**********************************************************************************/
#include "conf_manager.h"
#include "env_manager.h"
#include <iostream> 
#include <fstream> 
#include <boost/exception/all.hpp>
#include <vector>

namespace matrix
{
    namespace core
    {
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

            return E_SUCCESS;
        }

        int32_t conf_manager::parse_local_conf()
        {
            ////modify by regulus:fix validate port error. use string instead ulong in the options.
            //core opt description
            bpo::options_description core_opts("config file options");
            core_opts.add_options()
                ("host_ip", bpo::value<std::string>()->default_value(DEFAULT_LOCAL_IP), "")
                ("main_net_listen_port", bpo::value<std::string>()->default_value(DEFAULT_MAIN_NET_LISTEN_PORT), "")
                ("test_net_listen_port", bpo::value<std::string>()->default_value(DEFAULT_TEST_NET_LISTEN_PORT), "")
                ("container_ip", bpo::value<std::string>()->default_value(DEFAULT_LOCAL_IP), "")
                ("container_port", bpo::value<std::string>()->default_value(DEFAULT_CONTAINER_LISTEN_PORT), "")
                ("container_image", bpo::value<std::string>()->default_value(DEFAULT_CONTAINER_IMAGE_NAME), "");

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

            try
            {
                std::ifstream node_dat_ifs(node_dat_path.generic_string());
                bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), m_args);

                bpo::notify(m_args);
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "conf manager parse node.dat error: " << diagnostic_information(e);
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t conf_manager::init_params()
        {
            if (0 == m_args.count("node_id"))
            {
                LOG_ERROR << "conf_manager has no node id error";
                return E_DEFAULT;
            }
            m_node_id = m_args["node_id"].as<std::string>();


            if (0 == m_args.count("node_private_key"))
            {
                LOG_ERROR << "conf_manager has no node private key error";
                return E_DEFAULT;
            }
            m_node_private_key = m_args["node_private_key"].as<std::string>();

            return E_SUCCESS;
        }

        int32_t conf_manager::serialize_node_info(const node_info &info)
        {
            FILE *fp = nullptr;

            //node.dat path
            fs::path node_dat_path;
            node_dat_path /= fs::current_path();
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

            assert(nullptr != fp);

            fprintf(fp, "node_id=");
            fprintf(fp, info.node_id.c_str());
            fprintf(fp, "\n");

            fprintf(fp, "node_private_key=");
            fprintf(fp, info.node_private_key.c_str());
            fprintf(fp, "\n");

            if (fp)
            {
                fclose(fp);
            }
            return E_SUCCESS;
        }

    }

}