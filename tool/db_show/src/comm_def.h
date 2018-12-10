/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   byte_buf.h
* description    :   byte_buf for network transport
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include <iostream>
#include <string>
#include <assert.h>    
#include "leveldb/db.h"
#include <future>
#include <thread>
//#include "msg.h"
#include "protocol.h"
#include <unordered_map>
#include "byte_buf.h"
#include <memory>


enum peer_node_type
{
    NORMAL_NODE = 0,
    SEED_NODE = 1
};


enum training_task_status
{
    task_unknown = 1,
    task_queueing = 2,
    task_running = 4,
    task_stopped = 8,
    task_successfully_closed = 16,
    task_abnormally_closed = 32,
    task_overdue_close = 64
};

enum net_state
{
    ns_idle = 0,   //can use whenever needed
    ns_in_use,     //connecting or connected
    ns_failed,     //not use within a long time
    ns_zombie,
    ns_available
};


extern std::string to_node_type_string(int8_t status);
extern std::string to_training_task_status_string(int8_t status);
extern std::string to_net_state_string(int8_t status);