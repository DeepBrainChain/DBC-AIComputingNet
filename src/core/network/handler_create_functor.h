/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºhandler_create_functor.h
* description    £ºchannel handler create functor
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#include "channel.h"
#include "socket_channel_handler.h"


using handler_create_functor = std::function<matrix::core::socket_channel_handler * (matrix::core::channel *ch)>;
