#ifndef DBC_CMD_MESSAGE_H
#define DBC_CMD_MESSAGE_H

#include "util/utils.h"
#include "matrix_types.h"
#include "network/protocol/net_message.h"
#include "server/server.h"
#include "service/peer_request_service/peer_candidate.h"

using namespace boost::program_options;

// peer nodes
struct cmd_network_address {
    std::string ip;
    uint16_t port;
};

class cmd_peer_node_info {
public:
    std::string peer_node_id;

    int32_t live_time_stamp;

    int8_t net_st = -1;

    cmd_network_address addr;

    int8_t node_type = 0;

    std::vector<std::string> service_list;

    cmd_peer_node_info &operator=(const dbc::peer_node_info &info) {
        peer_node_id = info.peer_node_id;
        live_time_stamp = info.live_time_stamp;
        net_st = -1;
        addr.ip = info.addr.ip;
        addr.port = (uint16_t)info.addr.port;
        //node_type = info.
        service_list = info.service_list;
        return *this;
    }
};

class cmd_get_peer_nodes_req : public network::msg_base {
public:
    get_peers_flag flag;
};

class cmd_get_peer_nodes_rsp : public network::msg_base {
public:
    int32_t result;
    std::string result_info;
    std::vector<cmd_peer_node_info> peer_nodes_list;
};

#endif //DBC_MESSAGE_H
