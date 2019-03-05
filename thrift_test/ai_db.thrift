namespace cpp ai.dbc


//////////////////////////////////////////////////////////////////////////
//db.thrift is used to define db serialization format

struct ai_training_task{
  1: required string task_id,
  2: required i8 select_mode,
  3: optional string master,
  4: required list<string> peer_nodes_list,
  5: optional string server_specification,
  6: optional i32 server_count,
  7: required string training_engine,
  8: required string code_dir,
  9: required string entry_file,
  10: required string data_dir,
  11: required string checkpoint_dir
  12: optional string hyper_parameters
  13: required string ai_user_node_id
  14: required i64 start_time
  15: required i64 end_time
  
  252: required i8 error_times;
  253: required string container_id;
  254: required i64 received_time_stamp,
  255: required i8 status;
}

struct cmd_task_info{
	1: required string task_id,
    2: required i64 create_time,
    3: required string result,
	4: required i8 status,
	5: optional string description,
}
