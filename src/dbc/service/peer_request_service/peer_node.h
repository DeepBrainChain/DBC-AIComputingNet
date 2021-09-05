#pragma once

#include "util/utils.h"
#include "network/endpoint_address.h"
#include "network/socket_id.h"
#include "../message/matrix_types.h"

enum peer_node_type
{
    NORMAL_NODE = 0,
    SEED_NODE = 1
};

enum connection_status
{
    DISCONNECTED = 0,
    CONNECTED
};

class peer_node
{
    friend class p2p_net_service;

    friend void assign_peer_info(dbc::peer_node_info& info, const std::shared_ptr<peer_node>& node);

public:
    peer_node() = default;

    virtual ~peer_node() = default;

    peer_node(const peer_node &src);

    peer_node& operator=(const peer_node &src);

public:
    peer_node_type m_node_type = NORMAL_NODE;

    std::string m_id;
    dbc::network::socket_id m_sid;

    int32_t m_core_version = 0;
    int32_t m_protocol_version = 0;

    int64_t m_connected_time = 0;
    int64_t m_live_time = 0;

    connection_status m_connection_status = DISCONNECTED;

    dbc::network::endpoint_address m_peer_addr;
    dbc::network::endpoint_address m_local_addr;
};

extern void assign_peer_info(dbc::peer_node_info& info, const std::shared_ptr<peer_node>& node);
