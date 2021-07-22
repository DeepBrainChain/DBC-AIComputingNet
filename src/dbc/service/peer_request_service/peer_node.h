#pragma once

#include "util/utils.h"
#include "network/endpoint_address.h"
#include "network/socket_id.h"
#include "../message/matrix_types.h"

enum connection_status
{
    DISCONNECTED = 0,

    CONNECTED
};

class peer_node
{
    friend class p2p_net_service;

    friend void assign_peer_info(dbc::peer_node_info& info, const std::shared_ptr<peer_node> node);

public:

    peer_node();

    virtual ~peer_node() = default;

    peer_node(const peer_node &src);

    peer_node& operator=(const peer_node &src);

    std::string m_id;                               //peer node_id

    dbc::network::socket_id m_sid;                                //if connected directly, it has socket id

    int32_t m_core_version;

    int32_t m_protocol_version;

    int64_t m_connected_time;

    int64_t m_live_time;                            //active time, it doesn't work right now

    connection_status m_connection_status;

    dbc::network::endpoint_address m_peer_addr;

    dbc::network::endpoint_address m_local_addr;                   //local addr

    peer_node_type m_node_type;              //seed node or not

};

extern void assign_peer_info(dbc::peer_node_info& info, const std::shared_ptr<peer_node> node);
