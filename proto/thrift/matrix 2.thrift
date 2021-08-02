namespace cpp matrix.service_core

//////////////////////////////////////////////////////////////////////////
//msg header
//struct msg_header {
//  1: required i32 magic,
//  2: required string msg_name,
//  3: optional string nonce,
//  4: optional string session_id,
//  5: optional list<string> path,
//  255: optional map<string,string> exten_info
//}

//////////////////////////////////////////////////////////////////////////

//common structure
struct empty {

}

struct network_address {
  1: required string ip,
  2: required i16 port
}

struct peer_node_info {
  1: required string peer_node_id,
  2: required i32 core_version,
  3: required i32 protocol_version,
  4: required i32 live_time_stamp,
  5: required network_address addr,
  6: optional list<string> service_list
}

//////////////////////////////////////////////////////////////////////////
//shake hand
struct shake_hand_req {
  1: empty body
}

struct shake_hand_resp {
  1: empty body
}

//////////////////////////////////////////////////////////////////////////
//get_peer_nodes
struct get_peer_nodes_req {
  1: empty body
}

struct get_peer_nodes_resp_body {
  1: required list<peer_node_info> peer_nodes_list
}

struct get_peer_nodes_resp {
  1: get_peer_nodes_resp_body body
}

//////////////////////////////////////////////////////////////////////////
//peer_nodes_broadcast
struct peer_nodes_broadcast_req_body {
  1: required list<peer_node_info> peer_nodes_list
}

struct peer_nodes_broadcast_req {
  1: peer_nodes_broadcast_req_body body
}

//////////////////////////////////////////////////////////////////////////
// create task
// request
struct node_create_task_req_body {
  1: required list<string> peer_nodes_list,
  2: required string additional
}

struct node_create_task_req {
  1: node_create_task_req_body body
}
// response
struct node_create_task_rsp_body {
  1: required i32 result,
  2: required string result_msg,

  3: required string task_id,
  4: required string user_name,
  5: required string login_password,
  6: required string ip,
  7: required string ssh_port,
  8: required string create_time,
  9: required string system_storage,
  10: required string data_storage,
  11: required string cpu_cores,
  12: required string gpu_count,
  13: required string mem_size
}

struct node_create_task_rsp {
  1: node_create_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// start task
// request
struct node_start_task_req_body {
    1: required string task_id,
    2: required list<string> peer_nodes_list,
    3: required string additional
}

struct node_start_task_req {
  1: node_start_task_req_body body
}
// response
struct node_start_task_rsp_body {
  1: required i32 result,
  2: required string result_msg
}

struct node_start_task_rsp {
  1: node_start_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// stop task
// request
struct node_stop_task_req_body {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional
}

struct node_stop_task_req {
  1: node_stop_task_req_body body
}
// response
struct node_stop_task_rsp_body {
  1: required i32 result,
  2: required string result_msg
}

struct node_stop_task_rsp {
  1: node_stop_task_rsp_body body
}
////////////////////////////////////////////////////////////////////////////
// restart task
// request
struct node_restart_task_req_body {
    1: required string task_id,
    2: required list<string> peer_nodes_list,
    3: required string additional
}

struct node_restart_task_req {
  1: node_restart_task_req_body body
}
// response
struct node_restart_task_rsp_body {
  1: required i32 result,
  2: required string result_msg
}

struct node_restart_task_rsp {
  1: node_restart_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// reset task
// request
struct node_reset_task_req_body {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional
}

struct node_reset_task_req {
  1: node_reset_task_req_body body
}
// response
struct node_reset_task_rsp_body {
  1: required i32 result,
  2: required string result_msg
}

struct node_reset_task_rsp {
  1: node_reset_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// delete task
// request
struct node_delete_task_req_body {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional
}

struct node_delete_task_req {
  1: node_delete_task_req_body body
}
// response
struct node_delete_task_rsp_body {
  1: required i32 result,
  2: required string result_msg
}

struct node_delete_task_rsp {
  1: node_delete_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// task logs
// request
struct node_task_logs_req_body {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required i8 head_or_tail,
  4: required i16 number_of_lines,
  5: required string additional
}

struct node_task_logs_req {
  1: node_task_logs_req_body body
}
// response
struct node_task_logs_rsp_body {
  1: required i32 result,
  2: required string result_msg,
  3: required string log_content
}

struct node_task_logs_rsp {
  1: node_task_logs_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// list task
// request
struct node_list_task_req_body {
    1: required string task_id,
    2: required list<string> peer_nodes_list,
    3: required string additional
}

struct node_list_task_req {
  1: node_list_task_req_body body
}
// response
struct task_info {
  1: required string task_id,
  2: required i32 status,
  3: required string login_password
}

struct node_list_task_rsp_body {
  1: required i32 result,
  2: required string result_msg,
  3: required list<task_info> task_info_list
}

struct node_list_task_rsp {
  1: node_list_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// modify task
// request
struct node_modify_task_req_body {
    1: required string task_id,
    2: required list<string> peer_nodes_list,
    3: required string additional
}

struct node_modify_task_req {
  1: node_modify_task_req_body body
}
// response
struct node_modify_task_rsp_body {
  1: required i32 result,
  2: required string result_msg,
}

struct node_modify_task_rsp {
  1: node_modify_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
//version
struct ver_req_body {
  1: required string node_id,
  2: required i32 core_version,
  3: required i32 protocol_version,
  4: required i64 time_stamp,
  5: required network_address addr_me,
  6: required network_address addr_you,
  7: required i64 start_height
}

struct ver_req {
  1: ver_req_body body
}


struct ver_resp_body {
  1: required string node_id,
  2: required i32 core_version,
  3: required i32 protocol_version
}

struct ver_resp {
  1: ver_resp_body body
}

//////////////////////////////////////////////////////////////////////////
struct node_service_info {
  1: required list<string> service_list,
  2: optional string name,
  3: optional i64 time_stamp,
  4: optional map<string,string> kvs
}

// list nodes
struct node_query_node_info_req_body {
  1: required list<string> peer_nodes_list,
  2: required string additional
}

struct node_query_node_info_req {
  1: node_query_node_info_req_body body
}

struct node_query_node_info_rsp_body {
  1: required i32 result,
  2: required string result_msg,
  3: required map<string, string> kvs
}

struct node_query_node_info_rsp {
  1: node_query_node_info_rsp_body body
}

//node service broadcast
struct service_broadcast_req_body {
  1: required map<string, node_service_info> node_service_info_map
}

struct service_broadcast_req {
  1: service_broadcast_req_body body
}