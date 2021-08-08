#include "image_manager.h"
#include "util/crypto/utilstrencodings.h"
#include "log/log.h"

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

    util::read_file(file_name, out_info);
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
        m_c_pull_image = std::make_shared<bp::child>(pull_cmd, bp::std_out > file_name, bp::std_err > file_name);

        if (nullptr == m_c_pull_image || !m_c_pull_image->running())
        {
            LOG_ERROR << "start pulling image failed." << " image:" << image_name;
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
    m_c_pull_image = nullptr;
    return PULLING_ERROR;
}
