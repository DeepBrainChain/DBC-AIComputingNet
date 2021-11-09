namespace cpp dbc

//////////////////////////////////////////////////////////////////////////
// vm_task_thread_result
struct vm_task_thread_result {
    1: required string domain_name,     //任务虚拟机名称
    2: required i32 operation,          //任务操作类型
    3: required i32 result_code,        //执行结果代码
    4: required string result_message,  //执行结果消息体
    5: optional string vm_local_ip      //虚拟机本地IP地址
}
