namespace cpp dbc

struct TaskInfo {
    1: required string task_id,
    2: required string image_name, //镜像名字
    3: optional string login_password, //登录密码
    4: optional string ssh_port, //映射端口
    5: optional i32 operation, //操作
    6: optional i64 create_time, //创建时间
    7: optional i64 last_start_time, //上一次的启动时间
    8: optional i64 last_stop_time //上一次的关闭时间
}
