#include "peer_node.h"

peer_node::peer_node(const peer_node &src)
{
    *this = src;
}

peer_node& peer_node::operator=(const peer_node &src)
{
    if (this == &src)
    {
        return *this;
    }

    this->m_id = src.m_id;
    this->m_sid = src.m_sid;
    this->m_core_version = src.m_core_version;
    this->m_protocol_version = src.m_protocol_version;
    this->m_connected_time = src.m_connected_time;
    this->m_live_time = src.m_live_time;
    this->m_connection_status = src.m_connection_status;
    this->m_peer_addr = src.m_peer_addr;
    this->m_local_addr = src.m_local_addr;
    this->m_node_type = src.m_node_type;

    return *this;
}

void assign_peer_info(dbc::peer_node_info& info, const std::shared_ptr<peer_node>& node)
{
    info.peer_node_id = node->m_id;
    info.core_version = node->m_core_version;
    info.protocol_version = node->m_protocol_version;
    info.live_time_stamp = (int32_t)node->m_live_time;
    info.addr.ip = node->m_peer_addr.get_ip();
    info.addr.port = node->m_peer_addr.get_port();
}
