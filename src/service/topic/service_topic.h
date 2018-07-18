/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : topic.h
* description       : 
* date              : 2018/6/21
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include "service_message.h"

namespace service
{
class get_task_queue_size_req_msg: public matrix::core::message
    {
    public:
        get_task_queue_size_req_msg(){set_name(typeid(get_task_queue_size_req_msg).name());}
    };

class get_task_queue_size_resp_msg : public matrix::core::message
    {
    public:
        get_task_queue_size_resp_msg (): m_qsize(0) {set_name(typeid(get_task_queue_size_resp_msg).name());}
        uint32_t get() { return m_qsize;}
        void set(uint32_t n) { m_qsize = n;}
    private:
        uint32_t m_qsize;
    };

}