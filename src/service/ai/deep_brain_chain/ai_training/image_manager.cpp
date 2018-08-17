/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   container_client.cpp
* description    :   container client for definition
* date                  :   2018.08.15
* author            :   Regulus
**********************************************************************************/

#include "container_client.h"
#include "common.h"
#include "image_manager.h"
#include "util.h"
#include "utilstrencodings.h"
using namespace matrix::core;
namespace ai
{
    namespace dbc
    {

        image_manager::image_manager()
        {
            m_c_pull_image = nullptr;
        }

        std::string image_manager::get_out_log()
        {
            std::string file_name = "/tmp/" + m_image_name;
            
            std::string out_info;
            file_util::read_file(file_name, out_info);
            //std::string line;
            //int32_t i = 0;
            //while (out_stream && out_stream.cur != out_stream.end && std::getline(out_stream, line) && !line.empty() && i < 10)
            //{
            //    int64_t count = out_stream.gcount();
            //    out_info += (line+'\n');                
            //    i++;
            //}
            return out_info;
        }
        int32_t image_manager::start_pull(const std::string & image_name)
        {
            try
            {
                m_image_name = EncodeBase64(image_name);
                bp::ipstream err_stream;
                std::string pull_cmd = "docker pull " + image_name;
                //m_c_pull_image = std::make_shared<bp::child>(pull_cmd, bp::std_out > "/dev/null", bp::std_err > err_stream);
                std::string file_name = "/tmp/" + m_image_name;
                m_c_pull_image = std::make_shared<bp::child>(pull_cmd, bp::std_out > file_name, bp::std_err > err_stream);
                if (nullptr == m_c_pull_image || !m_c_pull_image->running())
                {
                    std::string err_info;
                    std::getline(err_stream, err_info);
                    if (!err_info.empty())
                    {
                        LOG_ERROR << "pull error. " << err_info << " image:" << image_name;
                    }
                    return E_DEFAULT;
                }
                return E_SUCCESS;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "pull image error: " << diagnostic_information(e) << " image:" << image_name;
                return E_DEFAULT;
            }
        }

        int32_t image_manager::terminate_pull()
        {
            if (m_c_pull_image->running())
            {
                m_c_pull_image->terminate();
            }
            return E_SUCCESS;
        }

        pull_state image_manager::check_pull_state()
        {
            if (nullptr == m_c_pull_image)
            {
                return PULLING_ERROR;
            }
            if (m_c_pull_image->running())
            {
                LOG_DEBUG << "running: docker pull " ;
                return PULLING;
            }

            LOG_INFO << "exit docker pull " << " exit code:" << m_c_pull_image->exit_code();
            if (0 == m_c_pull_image->exit_code())
            {
                LOG_DEBUG << "docker pull success";
                return PULLING_SUCCESS;
            }
            return PULLING_ERROR;
        }
    }

}