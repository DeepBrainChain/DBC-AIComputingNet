namespace cpp dbc

struct TaskIpTable {
    1: required string task_id,
    2: optional string host_ip,
    3: optional string vm_local_ip,
    4: optional string ssh_port
}

