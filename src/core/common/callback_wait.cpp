/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºcallback_wait.h
* description    £ºwait for a condition and execute call back function
* date                  : 2018.03.31
* author            £ºBruce Feng
**********************************************************************************/

#include "callback_wait.h"
#include "log.h"

namespace matrix
{
    namespace core
    {
        void default_dummy()
        {
            LOG_DEBUG << "callback_wait recieved notify msg";
        }

    }

}
