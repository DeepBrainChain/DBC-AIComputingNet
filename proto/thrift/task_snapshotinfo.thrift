namespace cpp dbc

//////////////////////////////////////////////////////////////////////////
// domain snapshot disk info
struct snapshotDiskInfo {
  1: required string name,         // 磁盘名称
  2: required string snapshot,     // 快照类型
  3: required string driver_type,  // 磁盘镜像类型
  4: optional string source_file   // 磁盘快照文件路径
}

//////////////////////////////////////////////////////////////////////////
// domain snapshot detail info
struct snapshotInfo {
  1: required string name,         // 快照名称
  2: required string description,  // 快照描述
  3: optional string state,        // 快照状态
  4: optional i64 creationTime,    // 创建时间
  5: optional list<snapshotDiskInfo> disks,  // 需要做快照的磁盘
  6: optional i32 error_code,                // 错误代码
  7: optional string error_message           // 错误信息
}
