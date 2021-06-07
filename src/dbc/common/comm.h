#ifndef DBC_COMM_H
#define DBC_COMM_H

#include <list>
#include <set>
#include "util.h"
#include <boost/program_options.hpp>
#include "time_util.h"
#include "log.h"

// sub operation of show cmd
enum {
    OP_SHOW_NODE_INFO = 0,
    OP_SHOW_SERVICE_LIST = 1,
    OP_SHOW_UNKNOWN = 0xff
};

class cmd_task_status {
public:

    std::string task_id;

    time_t create_time;

    int8_t status;

    std::string description;

    std::string raw;

    std::string pwd;
};

class cmd_peer_node_log {
public:

    std::string peer_node_id;

    std::string log_content;

};


#endif //DBC_COMM_H
