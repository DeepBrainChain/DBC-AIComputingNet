#ifndef DBC_MESSAGE_ID_H
#define DBC_MESSAGE_ID_H

// create task
#define NODE_CREATE_TASK_REQ        "node_create_task_req"
#define NODE_CREATE_TASK_RSP        "node_create_task_rsp"
#define NODE_CREATE_TASK_TIMER      "node_create_task_timer"

// start task
#define NODE_START_TASK_REQ     "node_start_task_req"
#define NODE_START_TASK_RSP     "node_start_task_rsp"
#define NODE_START_TASK_TIMER   "node_start_task_timer"

#define NODE_RESTART_TASK_REQ     "node_restart_task_req"
#define NODE_RESTART_TASK_RSP     "node_restart_task_rsp"

#define NODE_STOP_TASK_REQ      "node_stop_task_req"
#define NODE_STOP_TASK_RSP      "node_stop_task_rsp"

#define NODE_RESET_TASK_REQ      "node_reset_task_req"
#define NODE_RESET_TASK_RSP      "node_reset_task_rsp"

#define NODE_DESTROY_TASK_REQ      "node_destroy_task_req"
#define NODE_DESTROY_TASK_RSP      "node_destroy_task_rsp"

#define NODE_TASK_LOGS_REQ      "node_task_logs_req"
#define NODE_TASK_LOGS_RSP      "node_task_logs_rsp"

#define NODE_LIST_TASK_REQ      "node_list_task_req"
#define NODE_LIST_TASK_RSP      "node_list_task_rsp"

#define NODE_LIST_TASK_TIMER    "node_list_task_timer"
#define NODE_TASK_LOGS_TIMER    "node_task_logs_timer"


// def
#define LIST_ALL_TASKS            0
#define LIST_SPECIFIC_TASKS       1


#endif //DBC_MESSAGE_ID_H
