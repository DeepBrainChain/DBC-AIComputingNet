/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：conf_manager.h
* description    ：conf manager for core
* date                  : 2017.08.16
* author            ：Bruce Feng
**********************************************************************************/

#pragma once

#include "common.h"
#include "module.h"
#include "id_generator.h"


using namespace boost::program_options;


#define NODE_FILE_NAME                                                  "node.dat"
#define DEFAULT_MAIN_NET_LISTEN_PORT                11107
#define DEFAULT_TEST_NET_LISTEN_PORT                 21107
#define DEFAULT_CONTAINER_LISTEN_PORT              31107
#define DEFAULT_LOCAL_IP                                               "127.0.0.1"


extern const std::string conf_manager_name;

namespace matrix
{
    namespace core
    {

        class conf_manager : public module
        {
        public:

            virtual ~conf_manager() = default;

            virtual std::string module_name() const {return conf_manager_name;}

            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t exit() { m_args.clear(); return E_SUCCESS; }

            const variable_value& operator[](const std::string& name) const { return m_args[name]; }

            bool count(const std::string& name) const { return m_args.count(name);}

        public:

            const std::string & get_node_id() const { return m_node_id; }

            const std::string & get_node_private_key() const {return m_node_private_key;}

            static int32_t serialize_node_info(const node_info &info);

        protected:

            int32_t parse_local_conf();

            int32_t parse_node_dat();

            int32_t init_params();

        protected:

            variables_map m_args;

            std::string m_node_id;

            std::string m_node_private_key;
        };

    }

}




