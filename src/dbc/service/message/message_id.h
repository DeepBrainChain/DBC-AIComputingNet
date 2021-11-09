#ifndef DBC_MESSAGE_ID_H
#define DBC_MESSAGE_ID_H

#include <iostream>
#include <string>

// create task
#define NODE_CREATE_TASK_REQ        "node_create_task_req"
#define NODE_CREATE_TASK_RSP        "node_create_task_rsp"
#define NODE_CREATE_TASK_TIMER      "node_create_task_timer"

// start task
#define NODE_START_TASK_REQ     "node_start_task_req"
#define NODE_START_TASK_RSP     "node_start_task_rsp"
#define NODE_START_TASK_TIMER   "node_start_task_timer"

// stop task
#define NODE_STOP_TASK_REQ      "node_stop_task_req"
#define NODE_STOP_TASK_RSP      "node_stop_task_rsp"
#define NODE_STOP_TASK_TIMER    "node_stop_task_timer"

// restart task
#define NODE_RESTART_TASK_REQ     "node_restart_task_req"
#define NODE_RESTART_TASK_RSP     "node_restart_task_rsp"
#define NODE_RESTART_TASK_TIMER    "node_restart_task_timer"

// reset task
#define NODE_RESET_TASK_REQ      "node_reset_task_req"
#define NODE_RESET_TASK_RSP      "node_reset_task_rsp"
#define NODE_RESET_TASK_TIMER    "node_reset_task_timer"

// delete task
#define NODE_DELETE_TASK_REQ      "node_delete_task_req"
#define NODE_DELETE_TASK_RSP      "node_delete_task_rsp"
#define NODE_DELETE_TASK_TIMER    "node_delete_task_timer"

// task logs
#define NODE_TASK_LOGS_REQ      "node_task_logs_req"
#define NODE_TASK_LOGS_RSP      "node_task_logs_rsp"
#define NODE_TASK_LOGS_TIMER    "node_task_logs_timer"

// list task
#define NODE_LIST_TASK_REQ      "node_list_task_req"
#define NODE_LIST_TASK_RSP      "node_list_task_rsp"
#define NODE_LIST_TASK_TIMER    "node_list_task_timer"

// modify task
#define NODE_MODIFY_TASK_REQ      "node_modify_task_req"
#define NODE_MODIFY_TASK_RSP      "node_modify_task_rsp"
#define NODE_MODIFY_TASK_TIMER    "node_modify_task_timer"

// list nodes
#define NODE_QUERY_NODE_INFO_REQ       "node_query_node_info_req"
#define NODE_QUERY_NODE_INFO_RSP       "node_query_node_info_rsp"
#define NODE_QUERY_NODE_INFO_TIMER     "node_query_node_info_timer"

// get node session_id
#define NODE_SESSION_ID_REQ      "node_session_id_req"
#define NODE_SESSION_ID_RSP      "node_session_id_rsp"
#define NODE_SESSION_ID_TIMER    "node_session_id_timer"

// peer nodes


// service broadcast
#define SERVICE_BROADCAST_REQ          "service_broadcast_req"

// vm task thread result
#define VM_TASK_THREAD_RESULT      "vm_task_thread_result"

// compute node flag
#define COMPUTE_NODE_FLAG   "compute node"

// def
#define LIST_ALL_TASKS            0
#define LIST_SPECIFIC_TASKS       1

#define TASK_RESTART            "task_restart"
#define NODE_REBOOT             "node_reboot"

enum {
    MAX_NODE_INFO_MAP_SIZE = 64,
    MAX_SERVCIE_LIST_SIZE = 10,
    MAX_ATTR_STRING_LEN = 64,
    MAX_LONG_ATTR_STRING_LEN = 128,
    MAX_TIMESTAMP_DRIFT_IN_SECOND = 60,
    MAX_NDOE_INFO_KVS_SIZE = 10
};

#define TIMER_INTERVAL_NODE_INFO_COLLECTION           (20*1000)
#define NODE_INFO_COLLECTION_TIMER                    "node_info_collection_timer"
#define SERVICE_BROADCAST_TIMER                       "service_broadcast_timer"

static const int32_t MAX_NAME_STR_LENGTH = 16;


#endif //DBC_MESSAGE_ID_H
