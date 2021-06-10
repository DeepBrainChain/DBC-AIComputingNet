#ifndef DBC_MESSAGE_H
#define DBC_CMD_MESSAGE_H

#include "common/comm.h"
#include "task_common_def.h"
#include "console_printer.h"
#include "matrix_types.h"
#include "service_message.h"
#include "db/ai_db_types.h"
#include "callback_wait.h"
#include "server.h"
#include "api_call.h"
#include "peer_candidate.h"

using namespace boost::program_options;
using namespace matrix::core;
using namespace matrix::service_core;

class outputter {
public:
    virtual void format_output() {}
};

class cmd_get_peer_nodes_resp_formater : public outputter {
public:
    cmd_get_peer_nodes_resp_formater(std::shared_ptr<cmd_get_peer_nodes_resp> data) {
        m_data = data;
    };

    void format_output(matrix::service_core::get_peers_flag flag) {
        auto v = m_data.get();
        if (v == nullptr) {
            return;
        }

        dbc::console_printer printer;
        if (matrix::service_core::flag_active == flag) {
            //printer(LEFT_ALIGN, 64)(LEFT_ALIGN, 32)(LEFT_ALIGN, 10)(LEFT_ALIGN, 64);

            //printer << matrix::core::init << "peer_id" <<  "service_list" << matrix::core::endl;

            printer(dbc::LEFT_ALIGN, 32)(dbc::LEFT_ALIGN, 10)(dbc::LEFT_ALIGN, 10)(dbc::LEFT_ALIGN, 48);

            printer << dbc::init << "ip" << "port" << "nt" << "peer_id" << dbc::endl;

        } else if (matrix::service_core::flag_global == flag) {
            printer(dbc::LEFT_ALIGN, 32)(dbc::LEFT_ALIGN, 10)(dbc::LEFT_ALIGN, 10)(dbc::LEFT_ALIGN, 30)(dbc::LEFT_ALIGN, 48);

            printer << dbc::init << "ip" << "port" << "nt" << "status" << "peer_id"
                    << dbc::endl;
        }

        auto it = v->peer_nodes_list.begin();
        for (; it != v->peer_nodes_list.end(); ++it) {
            if (matrix::service_core::flag_active == flag) {
                //std::string svc_list;
                //for (auto its = it->service_list.begin(); its != it->service_list.end(); ++its)
                //{
                //    svc_list += *its;
                //    if (its + 1 != it->service_list.end())
                //    {
                //        svc_list += "|";
                //    }
                //}
                printer << dbc::init << string_util::fuzz_ip(
                        it->addr.ip) << it->addr.port << (int32_t) it->node_type
                        //<< svc_list
                        << it->peer_node_id << dbc::endl;
            } else if (matrix::service_core::flag_global == flag) {
                printer << dbc::init << string_util::fuzz_ip(
                        it->addr.ip) << it->addr.port << (int32_t) it->node_type
                        << matrix::service_core::net_state_2_string(
                                it->net_st) << (it->peer_node_id.empty() ? "N/A" : it->peer_node_id)
                        << dbc::endl;
            }
        }
    }

private:
    std::shared_ptr<cmd_get_peer_nodes_resp> m_data;
};

class cmd_start_training_req : public matrix::core::msg_base {
public:
    std::string task_file_path;

    std::map<std::string, std::string> parameters;

    bpo::variables_map vm;
};

class cmd_start_training_resp : public matrix::core::msg_base, public outputter {
public:
    int32_t result;
    std::string result_info;

    ai::dbc::cmd_task_info task_info;

    void format_output() {
        if (E_SUCCESS != result) {
            cout << result_info << endl;
            return;
        }

        cout << "task id: " << task_info.task_id << "       create_time: " << time_util::time_2_str(
                task_info.create_time) << endl;
    }
};

class cmd_stop_training_req : public matrix::core::msg_base {
public:
    std::string task_id;
};

class cmd_stop_training_resp : public matrix::core::msg_base, public outputter {
public:
    int32_t result;
    std::string result_info;

    void format_output() {
        if (E_SUCCESS != result) {
            cout << result_info << endl;
            return;
        }

        cout << "stopping training task......" << endl;
    }
};

class cmd_task_clean_req : public matrix::core::msg_base {
public:
    std::string task_id;
    bool clean_all;
};

class cmd_task_clean_resp : public matrix::core::msg_base, public outputter {
public:
    int32_t result;
    std::string result_info;

    void format_output() {
        if (E_SUCCESS != result) {
            cout << result_info << endl;
            return;
        }

        cout << "clean up training task......" << result_info << endl;
    }
};

class cmd_list_training_req : public matrix::core::msg_base {
public:

    int8_t list_type;                                   //0: list all tasks; 1: list specific tasks

    std::list<std::string> task_list;
};

class cmd_list_training_resp : public matrix::core::msg_base, public outputter {
public:

    int32_t result;
    std::string result_info;

    std::list<cmd_task_status> task_status_list;

    void format_output();

    void format_output_detail();

};

class cmd_logs_req : public matrix::core::msg_base {
public:

    std::string task_id;

    std::vector<std::string> peer_nodes_list;

    uint8_t head_or_tail;

    uint16_t number_of_lines;

    // for download training result
    std::string sub_op;
    std::string dest_folder;

};

class cmd_logs_resp : public matrix::core::msg_base, public outputter {
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

class cmd_show_req : public matrix::core::msg_base {
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
    cmd_show_req() :
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

class cmd_show_resp : public matrix::core::msg_base, public outputter {
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

    cmd_show_resp();

    void error(std::string err_);

    std::string to_string(std::vector<std::string> in);

    void format_service_list();

    void format_node_info();

    void format_output();

    std::string to_time_str(time_t t);

};

#endif //DBC_MESSAGE_H
