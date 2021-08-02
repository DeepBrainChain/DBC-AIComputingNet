#ifndef DBC_CONTAINER_RESOURCE_MNG_H
#define DBC_CONTAINER_RESOURCE_MNG_H

#include "util/utils.h"
#include <boost/process/cmd.hpp>
#include <boost/process/error.hpp>
#include <boost/process/system.hpp>
namespace bp = boost::process;

class container_resource_mng
{
public:
    container_resource_mng();
    int32_t init();
    int32_t exec_prune();
private:
    void set_prune_sh();

private:
    std::string m_prune_cmd;
    std::string m_docker_prune_sh_file_name;
};

#endif
