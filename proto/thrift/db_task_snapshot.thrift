namespace cpp dbc

struct snapshot_info {
    1: required string name,      //快照名字
    2: required string file,      //快照文件绝对路径
    3: required i64 create_time,  //创建时间
    4: required string desc       //描述
}

struct db_snapshot_info {
    1: required string task_id,
    2: required list<snapshot_info> snapshots
}
