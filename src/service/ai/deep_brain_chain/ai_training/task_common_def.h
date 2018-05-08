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

        std::string to_training_task_status_string(int8_t status);

    }

}