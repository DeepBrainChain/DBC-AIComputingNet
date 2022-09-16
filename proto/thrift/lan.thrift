namespace cpp dbc

//////////////////////////////////////////////////////////////////////////
struct multisig_sign_item {
  1: required string wallet,
  2: required string nonce,
  3: required string sign
}

//////////////////////////////////////////////////////////////////////////
// list local area network
// request
struct node_list_lan_req_data {
  1: required string network_id,
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
  12: optional string rent_order
}

struct node_list_lan_req_body {
    1: required string data
}

struct node_list_lan_req {
  1: node_list_lan_req_body body
}
// response
struct node_list_lan_rsp_body {
  1: required string data
}

struct node_list_lan_rsp {
  1: node_list_lan_rsp_body body
}

//////////////////////////////////////////////////////////////////////////
// create local area network
// request
struct node_create_lan_req_data {
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
  11: optional string rent_order
}

struct node_create_lan_req_body {
  1: required string data;
}

struct node_create_lan_req {
  1: node_create_lan_req_body body
}
// response
struct node_create_lan_rsp_body {
  1: required string data;
}

struct node_create_lan_rsp {
  1: node_create_lan_rsp_body body
}

//////////////////////////////////////////////////////////////////////////
// delete local area network
// request
struct node_delete_lan_req_data {
  1: required string network_id,
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
  12: optional string rent_order
}

struct node_delete_lan_req_body {
  1: required string data;
}

struct node_delete_lan_req {
  1: node_delete_lan_req_body body
}
// response
struct node_delete_lan_rsp_body {
  1: required string data;
}

struct node_delete_lan_rsp {
  1: node_delete_lan_rsp_body body
}
