namespace cpp matrix.service_core

//////////////////////////////////////////////////////////////////////////
//msg header
struct msg_header {
  1: required i32 magic,
  2: required string msg_name,
  3: optional string nonce,
  4: optional string session_id,
  255: optional map<string,string> exten_info
}

//////////////////////////////////////////////////////////////////////////

//common structure
struct empty {

}

struct network_address {
  1: required string ip,
  2: required i16 port
}

struct task_status {
  1: required string task_id,
  2: required i8 status
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
  1: msg_header header,
  2: ver_req_body body
}


struct ver_resp_body {
  1: required string node_id,
  2: required i32 core_version,
  3: required i32 protocol_version
}

struct ver_resp {
  1: msg_header header,
  2: ver_resp_body body
}

//////////////////////////////////////////////////////////////////////////
//shake hand

struct shake_hand_req {
  1: msg_header header,
  2: empty body
}

struct shake_hand_resp {
  1: msg_header header,
  2: empty body
}

//////////////////////////////////////////////////////////////////////////
//get_peer_nodes
struct get_peer_nodes_req {
  1: msg_header header
  2: empty body
}

struct get_peer_nodes_resp_body {
  1: required list<peer_node_info> peer_nodes_list
}

struct get_peer_nodes_resp {
  1: msg_header header,
  2: get_peer_nodes_resp_body body
}

//////////////////////////////////////////////////////////////////////////
//peer_nodes_broadcast
struct peer_nodes_broadcast_req_body {
  1: required list<peer_node_info> peer_nodes_list
}

struct peer_nodes_broadcast_req {
  1: msg_header header,
  2: peer_nodes_broadcast_req_body body
}

//////////////////////////////////////////////////////////////////////////
//start_training
struct start_training_req_body {
  1: required string task_id,
  2: required i8 select_mode,
  3: optional string master,
  4: optional list<string> peer_nodes_list,
  5: optional string server_specification,
  6: optional i32 server_count,
  7: required i32 training_engine,
  8: required string code_dir,
  9: required string entry_file,
  10: required string data_dir,
  11: required string checkpoint_dir
  12: optional string hyper_parameters
}

struct start_training_req {
  1: msg_header header,
  2: start_training_req_body body
}

//////////////////////////////////////////////////////////////////////////
//stop_training
struct stop_training_req_body {
  1: required string task_id
}

struct stop_training_req {
  1: msg_header header,
  2: stop_training_req_body body
}

//////////////////////////////////////////////////////////////////////////
//list_training
struct list_training_req_body {
  1: required list<string> task_list
}

struct list_training_req {
  1: msg_header header,
  2: list_training_req_body body
}

struct list_training_resp_body {
  1: required list<task_status> task_status_list
}

struct list_training_resp {
  1: msg_header header,
  2: list_training_resp_body body
}

//////////////////////////////////////////////////////////////////////////
//task logs
struct logs_req_body {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required i8 head_or_tail,
  4: required i16 number_of_lines
}

struct logs_req {
  1: msg_header header,
  2: logs_req_body body
}

struct peer_node_log {
  1: required string peer_node_id,
  2: required string log_content
}

struct logs_resp_body {
  1: required peer_node_log log
}

struct logs_resp {
  1: msg_header header,
  2: logs_resp_body body
}


//////////////////////////////////////////////////////////////////////////
//service definition
service dbc_service {

   ver_resp version(1:ver_req req),

   shake_hand_resp shake_hand(1: shake_hand_req req)
   
}


