#ifdef __RTX

#include "topic.h"

const topic_meta_data &topic::get_meta_data() const
{
    return m_meta_data;
}

const char * topic::get_topic_name() const
{
    return m_meta_data.topic_name;
}

uint32_t topic::get_topic_size() const
{
    return m_meta_data.topic_size;
}

const char * topic::get_topic_fields() const
{
    return m_meta_data.topic_fields;
}

#endif
