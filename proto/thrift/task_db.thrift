namespace cpp dbc

struct HardwareResource {
    1: optional i32 gpu_count,  //gpu数量
    2: optional i32 cpu_cores,  //cpu核心数量
    3: optional double mem_rate    //内存比率
}

struct TaskInfo {
    1: required string task_id,         //id
    2: optional string image_name,      //镜像名字
    3: optional string login_password,  //登录密码
    4: optional string ssh_port,        //映射端口
    5: optional i32 status,             //状态
    6: optional i32 operation,          //操作
    7: optional i64 create_time,        //创建时间
    8: optional i64 last_start_time,    //上一次的启动时间
    9: optional i64 last_stop_time,     //上一次的关闭时间
    10: HardwareResource hardware_resource,      //硬件资源（如：gpu、cpu）
    11: optional string vm_xml,         //自定义xml文件名
    12: optional string vm_xml_url,     //自定义xml文件下载地址
    13: optional string data_file_name, //数据磁盘镜像文件名
    14: optional string operation_system,  //操作系统(如：generic, ubuntu 18.04,windows 10)
    15: optional string bios_mode,      //BIOS 模式(如：legacy, uefi)
    16: optional string rdp_port,
    17: optional list<string> custom_port,  //自定义端口: "xx", "xx:yy", "xx-yy", "xx-yy:ww-pp"
    18: optional list<string> multicast //组播地址(如："230.0.0.1:5558")
}


struct rent_task {
    1: required string rent_wallet,
    2: required list<string> task_ids,
    3: required i64 rent_end
}
