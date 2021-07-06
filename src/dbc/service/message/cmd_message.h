#ifndef DBC_CMD_MESSAGE_H
#define DBC_CMD_MESSAGE_H

#include "common/comm.h"
#include "task_common_def.h"
#include "console_printer.h"
#include "matrix_types.h"
#include "service_message.h"
#include "task_types.h"
#include "callback_wait.h"
#include "server.h"
#include "api_call.h"
#include "peer_candidate.h"

using namespace boost::program_options;
using namespace matrix::core;
using namespace matrix::service_core;

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
    std::string login_password;
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

// destroy
class cmd_destroy_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_destroy_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;
};

// log
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

    std::vector<cmd_peer_node_log> peer_node_logs;

    // for download training result
    std::string sub_op;
    std::string dest_folder;

    // for log content check
    std::string task_id;

    // for duplicate log hiding
    // each log start with date info, like 2018-08-14T06:53:19.274586607Z
    enum {
        TIMESTAMP_STR_LENGTH = 30,
        IMAGE_HASH_STR_PREFIX_LENGTH = 12,
        IMAGE_HASH_STR_MAX_LENGTH = 64,
        MAX_NUM_IMAGE_HASH_LOG = 512

    };
    //bool no_duplicate;

    class series {
    public:
        std::string last_log_date;
        bool enable;
        std::set<std::string> image_download_logs;
    };

    static series m_series;

public:

    void format_output();

    std::string get_training_result_hash_from_log(std::string &s);

    std::string get_value_from_log(std::string prefix);

private:
    void format_output_segment();

    void format_output_series();

    void download_training_result();

    std::string get_log_date(std::string &s);


    bool is_image_download_str(std::string &s);

};

// list task
class cmd_list_task_req : public dbc::network::msg_base {
public:
    std::string task_id;
    std::vector<std::string> peer_nodes_list;
    std::string additional;
};

class cmd_list_task_rsp : public dbc::network::msg_base {
public:
    int32_t result;
    std::string result_info;

    std::list<cmd_task_status> task_status_list;

    void format_output();

    void format_output_detail();
};

// list peer_nodes
class cmd_get_peer_nodes_resp_formater {
public:
    cmd_get_peer_nodes_resp_formater(std::shared_ptr<cmd_get_peer_nodes_rsp> data) {
        m_data = data;
    };

private:
    std::shared_ptr<cmd_get_peer_nodes_rsp> m_data;
};

// mining_nodes
class cmd_list_node_req : public dbc::network::msg_base {
public:
    std::string o_node_id;
    std::string d_node_id;
    std::vector<std::string> keys;
    int32_t op;
    std::string filter;
    std::string sort;

    //todo: support offset, it is always 0 now
    int32_t num_lines;

public:
    cmd_list_node_req() :
            op(OP_SHOW_UNKNOWN), sort("gpu"), num_lines(1000) {

    }

    void set_sort(std::string f) {

        const char *fields[] = {"gpu", "name", "version", "state", "timestamp", "service", "startup_time"};

        int n = sizeof(fields) / sizeof(char *);
        bool found = false;
        for (int i = 0; (i < n && !found); i++) {
            if (f == std::string(fields[i])) {
                found = true;
            }
        }

        sort = found ? f : "gpu";
    }
};

class cmd_list_node_rsp : public dbc::network::msg_base {
public:
    std::string o_node_id;
    std::string d_node_id;
    std::map<std::string, std::string> kvs;
    std::shared_ptr<std::map<std::string, node_service_info> > id_2_services;

    int32_t op;
    std::string err;

    std::string filter;
    std::string sort;


public:

    cmd_list_node_rsp();

    void error(std::string err_);

    std::string to_string(std::vector<std::string> in);

    void format_service_list();

    void format_node_info();

    void format_output();

    std::string to_time_str(time_t t);

};

#endif //DBC_MESSAGE_H
