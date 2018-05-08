/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºtask_common_def.h
* description    £ºtask common definition
* date                  : 2018.01.28
* author            £ºBruce Feng
**********************************************************************************/

#include "task_common_def.h"


namespace ai
{
    namespace dbc
    {

        std::string to_training_task_status_string(int8_t status)
        {
            switch (status)
            {
            case task_unknown:
                return "task_unknown";
            case task_queueing:
                return "task_queueing";
            case task_running:
                return "task_running";
            case task_stopped:
                return "task_stopped";
            case task_succefully_closed:
                return "task_succefully_closed";
            case task_abnormally_closed:
                return "task_abnormally_closed";
            default:
                return DEFAULT_STRING;
            }
        }

    }

}