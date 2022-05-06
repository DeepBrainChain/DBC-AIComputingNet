namespace cpp dbc

struct db_task_iptable {
    1: required string task_id,
    2: optional string host_ip,
    3: optional string task_local_ip,
    4: optional string ssh_port,
    5: optional string rdp_port,
    6: optional list<string> custom_port,
    7: optional string public_ip
}