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

class get_task_queue_size_req_msg : public matrix::core::message {
public:
    get_task_queue_size_req_msg() { set_name(typeid(get_task_queue_size_req_msg).name()); }
};

class get_task_queue_size_resp_msg : public matrix::core::message {
public:
    get_task_queue_size_resp_msg() : m_qsize(0) { set_name(typeid(get_task_queue_size_resp_msg).name()); }

    int32_t get_task_size() { return m_qsize; }

    std::string get_gpu_state() { return m_gpu_state; }

    void set_task_size(int32_t n) { m_qsize = n; }

    void set_gpu_state(std::string s) { m_gpu_state = s; }

    std::string get_active_tasks() { return m_active_tasks; }

    void set_active_tasks(std::string s) { m_active_tasks = s; }

private:
    int32_t m_qsize;
    std::string m_gpu_state;
    std::string m_active_tasks;
};
