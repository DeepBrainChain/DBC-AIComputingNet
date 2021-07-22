#include "server_info.h"

server_info::server_info()
{
    m_service_list.clear();
}

server_info& server_info::operator = (const server_info& b)
{
    m_service_list = b.m_service_list;
    return *this;
}
