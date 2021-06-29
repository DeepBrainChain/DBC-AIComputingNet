#ifndef DBC_TASK_COMMON_DEF_H
#define DBC_TASK_COMMON_DEF_H

#include "common.h"

#define MAX_LIST_TASK_COUNT                                     10000
#define MAX_TASK_COUNT_PER_REQ                                  10
//TODO ...
#define MAX_TASK_COUNT_ON_PROVIDER                              10000
#define MAX_TASK_COUNT_IN_TRAINING_QUEUE                        1000
//#define MAX_TASK_COUNT_IN_LVLDB                               1000000
#define MAX_TASK_SHOWN_ON_LIST                                  100

#define GET_LOG_HEAD                                            0
#define GET_LOG_TAIL                                            1

//#define MAX_NUMBER_OF_LINES                                     500
#define MAX_NUMBER_OF_LINES                                     100
#define DEFAULT_NUMBER_OF_LINES                                 100

#define MAX_LOG_CONTENT_SIZE                                    (8 * 1024)

#define MAX_ENTRY_FILE_NAME_LEN                                 128
#define MAX_ENGINE_IMGE_NAME_LEN                                128

//#define LOG_AUTO_FLUSH_INTERVAL_IN_SECONDS                    10

#define AI_TRAINING_MAX_TASK_COUNT                                  64

namespace dbc
{
    enum ETaskStatus {
        T_OP_None,
        T_OP_Create, //创建中
        T_OP_Start, //启动中
        T_OP_Stop, //停止中
        T_OP_ReStart, //重启中
        T_OP_PullingImage //正在拉取景象
    };

    enum ETaskLogDirection {
        LD_Head,
        LD_Tail
    };






	enum TASK_STATE {
		DBC_TASK_RUNNING,
		DBC_TASK_NULL,
		DBC_TASK_NOEXIST,
		DBC_TASK_STOPPED
	};

	enum training_task_status
	{
		task_status_unknown = 1,
		task_status_queueing = 2,
		task_status_pulling_image = 3,
		task_status_running = 4,
		task_status_update_error = 5,  //代表从running的容器升级
		task_status_creating_image = 6,  //正在创建镜像
		//stop_update_task_error  =7,   //代表从stop的容器升级
		//termianl state
		task_status_stopped = 8,
		task_successfully_closed = 16,
		task_abnormally_closed = 32,
		task_overdue_closed = 64,
		task_noimage_closed = 65,
		task_nospace_closed = 66,
		task_status_out_of_gpu_resource = 67
	};

	std::string to_training_task_status_string(int8_t status);
	bool check_task_engine(const std::string& engine);
	void set_task_engine(std::string engine);
}

#endif
