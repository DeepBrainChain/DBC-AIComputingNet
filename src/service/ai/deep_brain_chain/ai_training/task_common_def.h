/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   task_common_def.h
* description    :   task common definition
* date                  :   2018.01.28
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include "common.h"


#define MAX_LIST_TASK_COUNT                                     100
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

#define LOG_AUTO_FLUSH_INTERVAL_IN_SECONDS                    10

namespace ai
{
    namespace dbc
    {

        enum training_task_status
        {
            task_unknown =       1,
            task_queueing =      2,
            task_pulling_image = 3,
            task_running = 4,

            ///////////termianl state///////////////////////
            task_stopped = 8,
            task_succefully_closed = 16,
            task_abnormally_closed = 32,
            task_overdue_closed     = 64,
            task_noimage_closed     = 65,
            task_nospace_closed     = 66
        };

        //__BEGIN_DECLS

        extern std::string to_training_task_status_string(int8_t status);
        extern bool check_task_engine(std::string engine);
        extern void set_task_engine(std::string engine);
        const std::string ECDSA = "ecdsa";
        //__END_DECLS;

    }

}