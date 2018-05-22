/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtask_common_def.h
* description    £ºtask common definition
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/

#pragma once


#include "common.h"


#define MAX_LIST_TASK_COUNT                                             100

#define GET_LOG_HEAD                                                                0
#define GET_LOG_TAIL                                                                  1

#define MAX_NUMBER_OF_LINES                                           500
#define DEFAULT_NUMBER_OF_LINES                                  100

#define MAX_LOG_CONTENT_SIZE                                          (8 * 1024)


namespace ai
{
    namespace dbc
    {

        enum training_task_status
        {
            task_unknown = 0,
            task_queueing,
            task_running,
            task_stopped,
            task_succefully_closed,
            task_abnormally_closed
        };

        //__BEGIN_DECLS

        extern std::string to_training_task_status_string(int8_t status);

        //__END_DECLS;

    }

}