namespace cpp dbc

struct db_bare_metal {
    1: required string node_id,
    2: required string node_private_key,
	3: required string uuid,              // 机器供应商的ID
	4: optional string desc,              // 机器描述
	5: optional string ipmi_hostname,     // Remote host name for LAN interface
	6: optional string ipmi_username,     // Remote session username
	7: optional string ipmi_password      // Remote session password
}
