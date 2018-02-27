/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：rtx_service_module.cpp
* description    ：service module on RTX platform
* date                  : 2018.01.20
* author            ：Bruce Feng
**********************************************************************************/
#ifdef __RTX
#include "rtx_service_node.h"
#include "service_bus.h"
#include "service_message_id.h"


__task void node_task(void *argv)
{
    ASSERT(argv);
    
    service_module *node = (service_module *)argv;
    //printf("node_task.\n");
		
    node->run();
}


service_module::service_module(service_node_meta_data *meta) : m_exited(false), m_meta(*meta), m_node_task(node_task)
{
    m_timer_manager = new TimerManager(this);
}

service_module::~service_module()
{
    //DELETE(m_meta);
    DELETE(m_timer_manager);
}

int32_t service_module::init()
{
    return service_init();
}

int32_t service_module::start()
{
    //create rtx task
    m_tid = os_tsk_create_user_ex(m_node_task, m_meta.task_priority, (void *)m_meta.stack, m_meta.stack_size, this);
    return 0 != m_tid ? E_SUCCESS : E_DEFAULT;
}

int32_t service_module::run()
{
    message_t message;
    SUBSCRIBER_HANDLE handle = NULL;

    //subscribe message topic
    handle = subscribe_topic(TOPIC_ID(message));

    //set max message count
    subscriber_meta_data meta;
    meta.max_mail_count_in_box = m_meta.max_message_count;
    set_subscriber(handle, &meta);

    bool updated = false;
    
    while(!m_exited)
    {
        //timer process
        m_timer_manager->process();
        
        //check topic data updated
        int32_t check_result = check_topic(handle, updated);
        if (updated)
        {
            //copy message topic data
            copy_topic_data(TOPIC_ID(message), handle, &message);

            //invoke with message
            on_invoke(&message);
        }
    }
    
    return E_SUCCESS;    
}

int32_t service_module::stop()
{
    m_exited = true;

    //have a rest
    OS_RESULT ret = os_tsk_delete(m_tid);
    return OS_R_OK == ret ? E_SUCCESS : E_DEFAULT;
}

int32_t service_module::exit()
{
    return service_exit();
}

int32_t service_module::on_invoke(message_t *message)
{
    ASSERT(message);
    
    switch (message->header.message_id)
    {
        case DEFAULT_MESSAGE_NAME:
            break;

        default:
            break;
    }

    return E_SUCCESS;
}

int32_t service_module::on_time_out(core_timer *timer)
{
    ASSERT(timer);

    switch (timer->get_name())
    {
        case DEFAULT_TIMER_NAME:
        break;

        default:
        break;
    }

    return E_SUCCESS;
}

#endif




