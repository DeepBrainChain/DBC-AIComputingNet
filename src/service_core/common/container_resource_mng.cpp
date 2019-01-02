/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : container_resource_mng.cpp
* description       : 
* date              : 2019/1/10
* author            : Regulus
**********************************************************************************/
#include <iostream>
#include <fstream>
#include "log.h"
#include "error.h"
#include "container_resource_mng.h"
#include <boost/format.hpp>
#include <boost/process/io.hpp>
#include "server.h"
#include <boost/process/async.hpp>

using namespace matrix::service_core;
using namespace matrix::core;


container_resource_mng::container_resource_mng():
    m_prune_cmd("")
    ,m_docker_prune_sh_file_name("")
{
}
int32_t container_resource_mng::init()
{
    int16_t prune_interver = CONF_MANAGER->get_prune_container_stop_interval();
    int16_t prune_usage = CONF_MANAGER->get_prune_docker_root_use_ratio();
    //minmum value is 1 hour, max value is 360 day.
    if (prune_interver < 1 || prune_interver > 8640)
    {
        LOG_ERROR << "prune_container_interval must bigger than 1h and smaller than 8640h";
        return E_DEFAULT;
    }

    //minmum value is 1 hour, max value is 360 day.
    if (prune_usage < 1 || prune_usage>100)
    {
        LOG_ERROR << "prune_container_freespace_scale must bigger than 1 and smaller than 100";
        return E_DEFAULT;
    }

    set_prune_sh();

#ifdef __linux__
    try
    {
        m_prune_cmd = boost::str(boost::format("/bin/bash %s %d %d") % m_docker_prune_sh_file_name % prune_interver % prune_usage);
    }
    catch (...)
    {
        return E_DEFAULT;
    }
#endif
    return E_SUCCESS;

}

int32_t container_resource_mng::exec_prune()
{
    if (m_prune_cmd.empty())
    {
        LOG_ERROR << "prune command is empty";
        return E_DEFAULT;
    }
    try
    {
        std::error_code ec;
        std::future<std::string> fut;
        std::future<std::string> fut_err;
        int32_t ret = bp::system(bp::cmd=m_prune_cmd, bp::std_out > fut, bp::std_err > fut_err, ec);
        std::string prune_log = "prune container info.";
        if (ec)
        {
            prune_log +=ec.message();
        }

        if (fut.valid())
        {
            prune_log += fut.get();
        }
        if (fut_err.valid())
        {
            prune_log += fut_err.get();
        }

        LOG_INFO << " prune ret code:" << ret << ". " << prune_log ;
    }
    catch (const std::exception & e)
    {
        LOG_ERROR << "prune container error" << e.what();
        return E_DEFAULT;
    }
    catch (...)
    {
        LOG_ERROR << "prune container error";
        return E_DEFAULT;
    }
    return E_SUCCESS;
}

void container_resource_mng::set_prune_sh()
{
    m_docker_prune_sh_file_name = env_manager::get_home_path().generic_string() + "/tool/rm_containers.sh";
}