namespace cpp dbc

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

//////////////////////////////////////////////////////////////////////////
//shake hand
struct shake_hand_req {
  1: empty body
}

struct shake_hand_resp {
  1: empty body
}
//////////////////////////////////////////////////////////////////////////
struct multisig_sign_item {
  1: required string wallet,
  2: required string nonce,
  3: required string sign
}
//////////////////////////////////////////////////////////////////////////
// list images
// request
struct node_list_images_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string image,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign,
  12: optional list<string> image_server
}

struct node_list_images_req_body {
    1: required string data
}

struct node_list_images_req {
  1: node_list_images_req_body body
}
// response
struct node_list_images_rsp_body {
  1: required string data
}

struct node_list_images_rsp {
  1: node_list_images_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// download image
// request
struct node_download_image_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string image,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign,
  12: optional list<string> image_server
}

struct node_download_image_req_body {
    1: required string data
}

struct node_download_image_req {
  1: node_download_image_req_body body
}
// response
struct node_download_image_rsp_body {
  1: required string data
}

struct node_download_image_rsp {
  1: node_download_image_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// upload image
// request
struct node_upload_image_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string image,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign,
  12: optional list<string> image_server
}

struct node_upload_image_req_body {
  1: required string data
}

struct node_upload_image_req {
  1: node_upload_image_req_body body
}
// response
struct node_upload_image_rsp_body {
  1: required string data
}

struct node_upload_image_rsp {
  1: node_upload_image_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// list task
// request
struct node_list_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_list_task_req_body {
    1: required string data
}

struct node_list_task_req {
  1: node_list_task_req_body body
}
// response
struct node_list_task_rsp_body {
  1: required string data
}

struct node_list_task_rsp {
  1: node_list_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// create task
// request
struct node_create_task_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string wallet,
  4: required string nonce,
  5: required string sign,
  6: required list<string> multisig_wallets,
  7: required i32 multisig_threshold,
  8: required list<multisig_sign_item> multisig_signs,
  9: required string session_id,
  10: required string session_id_sign,
  11: optional list<string> image_server,
  12: optional string custom_image_name
}

struct node_create_task_req_body {
  1: required string data;
}

struct node_create_task_req {
  1: node_create_task_req_body body
}
// response
struct node_create_task_rsp_body {
  1: required string data;
}

struct node_create_task_rsp {
  1: node_create_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// start task
// request
struct node_start_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_start_task_req_body {
    1: required string data
}

struct node_start_task_req {
  1: node_start_task_req_body body
}
// response
struct node_start_task_rsp_body {
  1: required string data
}

struct node_start_task_rsp {
  1: node_start_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// stop task
// request
struct node_stop_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_stop_task_req_body {
  1: required string data
}

struct node_stop_task_req {
  1: node_stop_task_req_body body
}
// response
struct node_stop_task_rsp_body {
  1: required string data
}

struct node_stop_task_rsp {
  1: node_stop_task_rsp_body body
}
////////////////////////////////////////////////////////////////////////////
// restart task
// request
struct node_restart_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign,
  12: optional i16 force_reboot
}

struct node_restart_task_req_body {
    1: required string data
}

struct node_restart_task_req {
  1: node_restart_task_req_body body
}
// response
struct node_restart_task_rsp_body {
  1: required string data
}

struct node_restart_task_rsp {
  1: node_restart_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// reset task
// request
struct node_reset_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_reset_task_req_body {
  1: required string data
}

struct node_reset_task_req {
  1: node_reset_task_req_body body
}
// response
struct node_reset_task_rsp_body {
  1: required string data
}

struct node_reset_task_rsp {
  1: node_reset_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// delete task
// request
struct node_delete_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_delete_task_req_body {
  1: required string data
}

struct node_delete_task_req {
  1: node_delete_task_req_body body
}
// response
struct node_delete_task_rsp_body {
  1: required string data
}

struct node_delete_task_rsp {
  1: node_delete_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// task logs
// request
struct node_task_logs_req_data {
  1: required string task_id,
  2: required i16 head_or_tail,
  3: required i32 number_of_lines,
  4: required list<string> peer_nodes_list,
  5: required string additional,
  6: required string wallet,
  7: required string nonce,
  8: required string sign,
  9: required list<string> multisig_wallets,
  10: required i32 multisig_threshold,
  11: required list<multisig_sign_item> multisig_signs,
  12: required string session_id,
  13: required string session_id_sign
}

struct node_task_logs_req_body {
  1: required string data
}

struct node_task_logs_req {
  1: node_task_logs_req_body body
}
// response
struct node_task_logs_rsp_body {
  1: required string data
}

struct node_task_logs_rsp {
  1: node_task_logs_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// modify task
// request
struct node_modify_task_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_modify_task_req_body {
    1: required string data
}

struct node_modify_task_req {
  1: node_modify_task_req_body body
}
// response
struct node_modify_task_rsp_body {
  1: required string data,
}

struct node_modify_task_rsp {
  1: node_modify_task_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// get node session_id
// request
struct node_session_id_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string wallet,
  4: required string nonce,
  5: required string sign,
  6: required list<string> multisig_wallets,
  7: required i32 multisig_threshold,
  8: required list<multisig_sign_item> multisig_signs
}

struct node_session_id_req_body {
  1: required string data
}

struct node_session_id_req {
  1: node_session_id_req_body body
}
// response
struct node_session_id_rsp_body {
  1: required string data
}

struct node_session_id_rsp {
  1: node_session_id_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// query node info
// request
struct node_query_node_info_req_data {
  1: required list<string> peer_nodes_list,
  2: required string additional,
  3: required string wallet,
  4: required string nonce,
  5: required string sign,
  6: required list<string> multisig_wallets,
  7: required i32 multisig_threshold,
  8: required list<multisig_sign_item> multisig_signs,
  9: required string session_id,
  10: required string session_id_sign,
  11: optional list<string> image_server
}

struct node_query_node_info_req_body {
  1: required string data
}

struct node_query_node_info_req {
  1: node_query_node_info_req_body body
}
// response
struct node_query_node_info_rsp_body {
  1: required string data
}

struct node_query_node_info_rsp {
  1: node_query_node_info_rsp_body body
}
//////////////////////////////////////////////////////////////////////////
// node service broadcast
// request
struct node_service_info {
  1: required list<string> service_list,
  2: optional string name,
  3: optional i64 time_stamp,
  4: optional map<string,string> kvs
}

struct service_broadcast_req_body {
  1: required map<string, node_service_info> node_service_info_map
}

struct service_broadcast_req {
  1: service_broadcast_req_body body
}
//////////////////////////////////////////////////////////////////////////
// request
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

struct peer_node_info {
  1: required string peer_node_id,
  2: required i32 core_version,
  3: required i32 protocol_version,
  4: required i32 live_time_stamp,
  5: required network_address addr,
  6: optional list<string> service_list
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
// list snapshot
// request
struct node_list_snapshot_req_data {
  1: required string task_id,
  2: required string snapshot_name,
  3: required list<string> peer_nodes_list,
  4: required string additional,
  5: required string wallet,
  6: required string nonce,
  7: required string sign,
  8: required list<string> multisig_wallets,
  9: required i32 multisig_threshold,
  10: required list<multisig_sign_item> multisig_signs,
  11: required string session_id,
  12: required string session_id_sign
}

struct node_list_snapshot_req_body {
    1: required string data
}

struct node_list_snapshot_req {
  1: node_list_snapshot_req_body body
}
// response
struct node_list_snapshot_rsp_body {
  1: required string data
}

struct node_list_snapshot_rsp {
  1: node_list_snapshot_rsp_body body
}

//////////////////////////////////////////////////////////////////////////
// create snapshot
// request
struct node_create_snapshot_req_data {
  1: required string task_id,
  2: required list<string> peer_nodes_list,
  3: required string additional,
  4: required string wallet,
  5: required string nonce,
  6: required string sign,
  7: required list<string> multisig_wallets,
  8: required i32 multisig_threshold,
  9: required list<multisig_sign_item> multisig_signs,
  10: required string session_id,
  11: required string session_id_sign
}

struct node_create_snapshot_req_body {
  1: required string data;
}

struct node_create_snapshot_req {
  1: node_create_snapshot_req_body body
}
// response
struct node_create_snapshot_rsp_body {
  1: required string data;
}

struct node_create_snapshot_rsp {
  1: node_create_snapshot_rsp_body body
}

//////////////////////////////////////////////////////////////////////////
// delete snapshot
// request
struct node_delete_snapshot_req_data {
  1: required string task_id,
  2: required string snapshot_name,
  3: required list<string> peer_nodes_list,
  4: required string additional,
  5: required string wallet,
  6: required string nonce,
  7: required string sign,
  8: required list<string> multisig_wallets,
  9: required i32 multisig_threshold,
  10: required list<multisig_sign_item> multisig_signs,
  11: required string session_id,
  12: required string session_id_sign
}

struct node_delete_snapshot_req_body {
  1: required string data
}

struct node_delete_snapshot_req {
  1: node_delete_snapshot_req_body body
}
// response
struct node_delete_snapshot_rsp_body {
  1: required string data
}

struct node_delete_snapshot_rsp {
  1: node_delete_snapshot_rsp_body body
}

