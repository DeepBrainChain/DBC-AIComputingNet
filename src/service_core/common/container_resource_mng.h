/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : container_resource_mng.h
* description       : 
* date              : 2019/1/10
* author            : Regulus
**********************************************************************************/
#pragma once

#include <string>
#include <time.h>
#include <boost/process/cmd.hpp>
#include <boost/process/error.hpp>
#include <boost/process/system.hpp>
namespace bp = boost::process;

namespace matrix
{
    namespace service_core
    {
        class container_resource_mng
        {
        public:
            container_resource_mng();
            int32_t init();
            int32_t exec_prune();
        private:
            void set_prune_sh();
            std::string del_images();
        private:
            std::string m_prune_cmd;
            std::string m_docker_prune_sh_file_name;
        };
    }
}