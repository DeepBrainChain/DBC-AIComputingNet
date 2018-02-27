#ifdef __RTX

#include "topic_entry.h"
#include "publisher.h"
#include "subscriber.h"


topic_entry::~topic_entry()
{
    remove_all_publishers();
    remove_all_subscribers();
}

topic *topic_entry::get_topic()
{
    return &m_topic;
}

/*
char *topic_entry::get_topic_data(int instance_number)
{
    m_topic_mailbox.recv(instance_number);
}*/

list<publisher *> &topic_entry::get_publisers()
{
    return m_publishers;
}

list<subscriber *> &topic_entry::get_subscribers()
{
    return m_subscribers;
}

int32_t topic_entry::add_publisher(publisher * publisher)
{
    if (NULL == publisher)
    {
        return E_BAD_PARAM;
    }

    m_publishers.add_tail(publisher);       //是否需要查找是否已经存在
    return E_SUCCESS;
}

int32_t topic_entry::add_subscriber(subscriber * subscriber)
{
    if (NULL == subscriber)
    {
        return E_BAD_PARAM;
    }

    m_subscribers.add_tail(subscriber);       //是否需要查找是否已经存在
    return E_SUCCESS;
}

int32_t topic_entry::remove_publisher(publisher * publisher)
{
    publisher *publisher_founded = NULL;
    POSITION pos_founded = NULL;
    
    POSITION pos = m_publishers.get_head_position();
    while(pos)
    {
        pos_founded = pos;
        publisher_founded = m_publishers.get_next(pos);

        if (publisher_founded == publisher)
        {
            m_publishers.remove_at(pos_founded);
            DELETE(publisher_founded);
        
            return E_SUCCESS;
        }
    }

    return E_DEFAULT;

}


int32_t topic_entry::remove_subscriber(subscriber * subscriber)
{
    subscriber *subscriber_founded = NULL;
    POSITION pos_founded = NULL;
    
    POSITION pos = m_subscribers.get_head_position();
    while(pos)
    {
        pos_founded = pos;
        subscriber_founded = m_subscribers.get_next(pos);

        if (subscriber_founded == subscriber)
        {
            m_subscribers.remove_at(pos_founded);
            DELETE(subscriber_founded);
        
            return E_SUCCESS;
        }
    }

    return E_DEFAULT;
}


int32_t topic_entry::send_to_subscribers(const void *topic_data)
{
    subscriber *subscriber = NULL;
    
    POSITION pos = m_subscribers.get_head_position();
    while(pos)
    {
        subscriber = m_subscribers.get_next(pos);

        if (subscriber)
        {
            subscriber->send(topic_data);
            subscriber->notify_topic();
        }
    }

    return E_SUCCESS;
}

void topic_entry::remove_all_publishers()
{
    publisher *publisher = NULL;
    
    POSITION pos = m_publishers.get_head_position();
    while(pos)
    {
        publisher = m_publishers.get_next(pos);
        
        DELETE(publisher);
    }

    m_publishers.remove_all();
}

void topic_entry::remove_all_subscribers()
{
    subscriber *subscriber = NULL;
    
    POSITION pos = m_subscribers.get_head_position();
    while(pos)
    {
        subscriber = m_subscribers.get_next(pos);
        
        DELETE(subscriber);
    }

    m_subscribers.remove_all();
}

#endif


