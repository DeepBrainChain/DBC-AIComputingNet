namespace cpp dbc

struct db_bare_metal {
    1: required string node_id,
    2: required string node_private_key,
    3: required string uuid,                         // 机器供应商的ID
    4: required string ip,                           // 机器IP地址
    5: optional string os,                           // 机器操作系统
    6: optional string desc,                         // 机器描述
    7: optional string ipmi_hostname,                // Remote host name for LAN interface
    8: optional string ipmi_username,                // Remote session username
    9: optional string ipmi_password,                // Remote session password
    10: optional string ipmi_port,                   // Remote RMCP port
    20: optional string deeplink_device_id,          // DeepLink 设备码
    21: optional string deeplink_device_password     // DeepLink 设备识别码
}
