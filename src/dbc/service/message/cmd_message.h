#ifndef DBC_CMD_MESSAGE_H
#define DBC_CMD_MESSAGE_H

#include "util/utils.h"
#include "../http_request_service/console_printer.h"
#include "matrix_types.h"
#include "network/protocol/service_message.h"
#include "data/task/db/task_db_types.h"
#include "server/server.h"
#include "service/peer_request_service/peer_candidate.h"

using namespace boost::program_options;

// create task
class cmd_create_task_req : public dbc::network::msg_base {
public:
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_create_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;

    std::string task_id;
    std::string user_name;
    std::string login_password;
    std::string ip;
    std::string ssh_port;
    std::string create_time;
    std::string system_storage;
    std::string data_storage;
    std::string cpu_cores;
    std::string gpu_count;
    std::string mem_size;
};

// start task
class cmd_start_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_start_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// stop
class cmd_stop_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_stop_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// restart task
class cmd_restart_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_restart_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// reset
class cmd_reset_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_reset_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// delete
class cmd_delete_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_delete_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// task log
class cmd_task_logs_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    uint8_t head_or_tail;
    uint16_t number_of_lines;
    std::string additional;
};

class cmd_task_logs_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
    std::string log_content;
};

// list task
class cmd_list_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_task_info {
public:
    std::string task_id;
    time_t create_time;
    int32_t status;
    std::string description;
    std::string raw;
    std::string pwd;
};

class cmd_list_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
    std::list<cmd_task_info> task_info_list;
};

// task modify
class cmd_modify_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_modify_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// mining_nodes
class cmd_list_node_req : public dbc::network::msg_base {
public:
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_list_node_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
    std::map<std::string, std::string> kvs;
    std::shared_ptr<std::map<std::string, dbc::node_service_info> > id_2_services;

    std::string to_string(std::vector<std::string> in);
};

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

class cmd_get_peer_nodes_req : public dbc::network::msg_base {
public:
    get_peers_flag flag;
};

class cmd_get_peer_nodes_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
    std::vector<cmd_peer_node_info> peer_nodes_list;
};

#endif //DBC_MESSAGE_H
