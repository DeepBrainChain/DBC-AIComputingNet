/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   service_message.cpp
* description    :   service message
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#include "service_message.h"

#ifdef __RTX

//定义总线消息
TOPIC_DEFINE(/service_bus/, message, message_t, "");

#endif


