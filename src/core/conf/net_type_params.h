/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   net_type_param.h
* description    :   net type params for config, main net or test net
* date                  :   2018.05.26
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include "common.h"
#include <boost/program_options.hpp>


using namespace boost::program_options;


namespace matrix
{
    namespace core
    {

        class net_type_params
        {
        public:

            net_type_params() = default;

            virtual ~net_type_params() = default;

            virtual const std::string & get_net_listen_port() { return m_net_listen_port; }

            virtual void set_net_listen_port(const std::string  & net_listen_port) { m_net_listen_port = net_listen_port; }

        protected:

            std::string m_net_listen_port;

        };

    }

}