namespace cpp dbc

struct db_peer_candidate {
    1: required string ip,
    2: required i16 port,
    3: required i8 net_state,
    4: required i32 reconn_cnt,
    5: required i64 last_conn_tm,
    6: required i32 score,
    7: required string node_id,
    8: required i8 node_type
}
