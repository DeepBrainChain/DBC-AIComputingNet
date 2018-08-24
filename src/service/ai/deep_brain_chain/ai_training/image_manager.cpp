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

        std::string image_manager::get_out_log(const std::string & image_name)
        {
            std::string out_info = "";
            if (image_name != m_image_name)
            {
                return out_info;
            }
            std::string file_name = "/tmp/" + EncodeBase64(m_image_name);
            
            file_util::read_file(file_name, out_info);
            return out_info;
        }

        int32_t image_manager::start_pull(const std::string & image_name)
        {
            try
            {
                m_image_name = image_name;
                //bp::ipstream err_stream;
                std::string pull_cmd = "docker pull " + image_name;
                //m_c_pull_image = std::make_shared<bp::child>(pull_cmd, bp::std_out > "/dev/null", bp::std_err > err_stream);

                std::string file_name = "/tmp/" + EncodeBase64(m_image_name);
                m_c_pull_image = std::make_shared<bp::child>(pull_cmd, bp::std_out > file_name, bp::std_err > m_err_stream);

                if (nullptr == m_c_pull_image || !m_c_pull_image->running())
                {
                    LOG_ERROR << "start pulling image failed." << " image:" << image_name;
                    std::string err_info = "";
                    std::string line;
                    while (m_err_stream && std::getline(m_err_stream, line) && !line.empty())
                    {
                        err_info += line;
                    }

                    if (!err_info.empty())
                    {
                        LOG_ERROR << "docker pull image failed. image:" << m_image_name << "error info:" << err_info;
                    }
                    return E_DEFAULT;
                }
                LOG_INFO << "start pull engine image:" << image_name << " pull image process pid:" << m_c_pull_image->id();
                return E_SUCCESS;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "pull image error: " << diagnostic_information(e) << " image:" << image_name;
                return E_DEFAULT;
            }
            catch (...)
            {
                LOG_ERROR << "pull image error: " <<  " image:" << image_name;
                return E_DEFAULT;
            }
        }

        int32_t image_manager::terminate_pull()
        {
            try
            {
                if (nullptr == m_c_pull_image)
                {
                    return E_SUCCESS;
                }

                if (m_c_pull_image->running())
                {
                    m_c_pull_image->terminate();
                }

                m_c_pull_image = nullptr;
            }
            catch (const boost::exception & e)
            {
                LOG_ERROR << "terminate pull image error: " << diagnostic_information(e) << " image:" << m_image_name;
                return E_DEFAULT;
            }
            catch (...)
            {
                LOG_ERROR << "terminate pull image error: " << " image:" << m_image_name;
                return E_DEFAULT;
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

            LOG_INFO << "exit docker pull. " << " exit code:" << m_c_pull_image->exit_code();
            if (0 == m_c_pull_image->exit_code())
            {
                LOG_DEBUG << "docker pull success.image:" << m_image_name;
                m_c_pull_image = nullptr;
                return PULLING_SUCCESS;
            }
            else
            {
                std::string err_info = "";
                std::string line;
                while (m_err_stream && std::getline(m_err_stream, line) && !line.empty())
                {
                    err_info += line;
                }

                if (!err_info.empty())
                {
                    LOG_ERROR << "docker pull image failed. image:" << m_image_name << "error info:" << err_info;
                }
            }
            m_c_pull_image = nullptr;
            return PULLING_ERROR;
        }
    }

}