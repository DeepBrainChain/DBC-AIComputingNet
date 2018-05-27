/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   topic_manager.cpp
* description    :   topic manager
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#include "topic_manager.h"
#ifdef __RTX
#include "mutex.h"
#include "topic_entry.h"
#include "publisher.h"
#include "subscriber.h"


topic_manager *topic_manager::m_instance = NULL;


int32_t topic_manager::init()
{
    AutoMutex<Mutex> auto_mutex(&m_mutex);   
    return m_registry.init();
}

PUBLISHER_HANDLE topic_manager::register_topic(const topic_meta_data *meta)
{
    if (NULL == meta)
    {
        return NULL;
    }

    AutoMutex<Mutex> auto_mutex(&m_mutex);   

    //create topic entry
    topic_entry *entry = m_registry.get_topic_entry(meta->topic_name);
    if (NULL == entry)
    {
        entry = m_registry.create_topic_entry(meta);
        m_registry.register_topic_entry(meta->topic_name, entry);
    }

    //create publisher
    publisher *publisher = new publisher(entry);
    entry->add_publisher(publisher);

    return (PUBLISHER_HANDLE)publisher;
}

int32_t topic_manager::publish_topic_data(PUBLISHER_HANDLE handle, const void *topic_data)
{
    if (NULL == handle  || NULL == topic_data)
    {
        return E_BAD_PARAM;
    }

    //get topic entry
    publisher *publisher = (publisher *)handle;
    topic_entry *entry = publisher->get_topic_entry();
    if (NULL == entry)
    {
        return E_NULL_POINTER;
    }

    //send to all subscribers
    return entry->send_to_subscribers(topic_data);
}

SUBSCRIBER_HANDLE topic_manager::subscribe_topic(const topic_meta_data *meta)
{
    if (NULL == meta || NULL == meta->topic_name)
    {
        return NULL;
    }

    AutoMutex<Mutex> auto_mutex(&m_mutex);
    topic_entry *entry = m_registry.get_topic_entry(meta->topic_name);
    if (NULL == entry)
    {
        entry = m_registry.create_topic_entry(meta);
        m_registry.register_topic_entry(meta->topic_name, entry);
    }

    subscriber *subscriber = new subscriber(entry);
    entry->add_subscriber(subscriber);
    
    return (SUBSCRIBER_HANDLE)subscriber;
}

int32_t topic_manager::unsubscribe_topic(SUBSCRIBER_HANDLE handle)
{
    if (NULL == handle)
    {
        return E_BAD_PARAM;
    }

    subscriber *subscriber = (subscriber *)handle;
    topic_entry *entry = subscriber->get_topic_entry();

    if (NULL == entry)
    {
        return E_NULL_POINTER;
    }

    return entry->remove_subscriber((subscriber *)handle);
}

int32_t topic_manager::check_topic(SUBSCRIBER_HANDLE handle, bool &updated)
{
    if (NULL == handle)
    {
        return E_BAD_PARAM;
    }

    subscriber *subscriber = (subscriber *)handle;
    updated = subscriber->is_updated() ? true : false;

    return E_SUCCESS;
}

bool topic_manager::exists_topic(const topic_meta_data *meta)
{
    if (NULL == meta || NULL == meta->topic_name)
    {
        return false;
    }

    AutoMutex<Mutex> auto_mutex(&m_mutex);
    return NULL != m_registry.get_topic_entry(meta->topic_name) ? true : false;
}

int32_t topic_manager::copy_topic_data(const topic_meta_data *meta, SUBSCRIBER_HANDLE handle, void *buffer)
{
    if (NULL == meta || NULL == handle || NULL == buffer)
    {
        return E_BAD_PARAM;
    }

    subscriber *subscriber = (subscriber *)handle;
    topic_entry *entry = subscriber->get_topic_entry();

    if (NULL == entry)
    {
        return E_NULL_POINTER;
    }

    if (subscriber->is_updated())
    {
        subscriber->recv(buffer);
        return E_SUCCESS;
    }
    else
    {
        return E_DEFAULT;
    }
}

int32_t topic_manager::set_subscriber(SUBSCRIBER_HANDLE handle, const subscriber_meta_data *meta)
{
    if (NULL == meta || NULL == handle)
    {
        return E_BAD_PARAM;
    }

    subscriber *subscriber = (subscriber *)handle;
    int32_t ret = E_SUCCESS;

    if (meta->max_mail_count_in_box >= 1)
    {
        ret = subscriber->set_mail_count(meta->max_mail_count_in_box);
    }
    
    return ret;
}

#else



#endif




