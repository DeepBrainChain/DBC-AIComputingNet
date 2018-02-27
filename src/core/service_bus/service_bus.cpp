#ifdef __RTX

#include "service_bus.h"
#include "topic_manager.h"
#include "subscriber.h"


PUBLISHER_HANDLE register_topic(const topic_meta_data *meta)
{
    if (NULL == meta)
    {
        return NULL;
    }
    
    return topic_manager::get_instance()->register_topic(meta);
}

int32_t publish_topic_data(PUBLISHER_HANDLE handle, const void *topic_data)
{
    if (NULL == handle || NULL == topic_data)
    {
        return E_BAD_PARAM;
    }

    return topic_manager::get_instance()->publish_topic_data(handle, topic_data);
}

int32_t unregister_topic(PUBLISHER_HANDLE handle)
{
    return E_SUCCESS;
}

SUBSCRIBER_HANDLE subscribe_topic(const topic_meta_data *meta)
{
    if (NULL == meta)
    {
        return NULL;
    }

    return topic_manager::get_instance()->subscribe_topic(meta);
}

int32_t unsubscribe_topic(SUBSCRIBER_HANDLE handle)
{
//    if (NULL == handle)
//    {
//        return E_BAD_PARAM;
//    }
//
//    return topic_manager::get_instance()->unsubscribe_topic(handle);

    return E_SUCCESS;
}

int32_t poll_topic(SUBSCRIBER_HANDLE handle, uint16_t timeout)
{
    if (NULL == handle)
    {
        return E_BAD_PARAM;
    }

    subscriber *subscriber_ptr = (subscriber *)handle;
    if (subscriber_ptr->is_updated())
    {
        return E_SUCCESS;
    }
    return subscriber->wait_topic(timeout);
}

int32_t check_topic(SUBSCRIBER_HANDLE handle, bool &updated)
{
    if (NULL == handle)
    {
        return E_BAD_PARAM;
    }

    return topic_manager::get_instance()->check_topic(handle, updated);
}

bool exists_topic(const topic_meta_data *meta)
{
    if (NULL == meta)
    {
        return NULL;
    }

    return topic_manager::get_instance()->exists_topic(meta);
}

int32_t copy_topic_data(const topic_meta_data *meta, SUBSCRIBER_HANDLE handle, void *buffer)
{
    if (NULL == handle || NULL == meta || NULL == buffer)
    {
        return E_BAD_PARAM;
    }

    return topic_manager::get_instance()->copy_topic_data(meta, handle, buffer);
}


int32_t set_subscriber(SUBSCRIBER_HANDLE handle, const subscriber_meta_data *meta)
{
    if (NULL == handle || NULL == meta)
    {
        return E_BAD_PARAM;
    }

    return topic_manager::get_instance()->set_subscriber(handle, meta);
}

#endif



