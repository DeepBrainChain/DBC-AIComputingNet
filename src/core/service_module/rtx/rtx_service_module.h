/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   rtx_service_module.h
* description    :   service module on RTX platform
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/

#ifndef _RTX_SERVICE_NODE_H_
#define _RTX_SERVICE_NODE_H_

#ifdef __RTX

#include <rtl.h>

#include "common.h"
#include "service_message.h"
#include "timer.h"
#include "timer_manager.h"


#define DEFAULT_MAX_MESSAGE_COUNT  1



typedef void (*RTX_NODE_TASK_FUNC)(void *);



struct service_node_meta_data
{
    uint8_t task_priority;

    void *stack;
    uint16_t stack_size;

    uint32_t max_message_count;
};


__BEGIN_DECLS

extern __PRIVATE void node_task(void *argv);

__END_DECLS;


class TimerManager;


class __EXPORT  service_module
{
public:

    service_module(service_node_meta_data *meta);
    
    virtual ~service_module();

    virtual int32_t init();

    virtual int32_t start();

    virtual int32_t run();

    virtual int32_t stop();

    virtual int32_t exit();

    virtual int32_t on_time_out(core_timer *timer);

protected:

    //业务层override
    
    virtual int32_t service_init() {return E_SUCCESS;}

    virtual int32_t service_exit() {return E_SUCCESS;}

    virtual int32_t on_invoke(message_t *message);

protected:

    OS_TID m_tid;

    bool m_exited;

    service_node_meta_data m_meta;    

    RTX_NODE_TASK_FUNC m_node_task;

    TimerManager *m_timer_manager;
    
};

#endif

#endif



