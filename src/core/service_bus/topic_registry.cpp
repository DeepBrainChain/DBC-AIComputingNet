#ifdef __RTX
#include "topic_registry.h"


topic_registry::~topic_registry()
{
    clear();
}

int32_t topic_registry::init()
{
    m_entries.init_hash_table();

    return E_SUCCESS;
}

topic_entry *topic_registry::get_topic_entry(const char * topic_name)
{
    if (NULL == topic_name)
    {
        return NULL;
    }

    topic_entry *entry = NULL;
    if (m_entries.find((char *)topic_name, entry))
    {
        return entry;
    }
    else
    {
        return NULL;
    }       
}

list<publisher *> *topic_registry::get_publishers(const char * topic_name)
{
    topic_entry *entry = get_topic_entry((char *)topic_name);
    return (NULL == entry) ? NULL : &entry->get_publisers();
}

list<subscriber *> *topic_registry::get_subscribers(const char * topic_name)
{
    topic_entry *entry = get_topic_entry(topic_name);
    return (NULL == entry) ? NULL : &entry->get_subscribers();
}

topic_entry * topic_registry::create_topic_entry(const topic_meta_data *meta, uint32_t max_instance_count)
{
    if (NULL == meta)
    {
        return NULL;
    }
    
    topic_entry *entry = new topic_entry(meta);
    //m_entries.set_at(meta->topic_name, entry);

    return entry;
}

void topic_registry::register_topic_entry(const char *topic_name, topic_entry *entry)
{
    m_entries.set_at((char *)topic_name, entry);
}

int32_t topic_registry::register_publisher(const char *topic_name, publisher * publisher)
{
    topic_entry *entry = NULL;
    if (false == m_entries.find((char *)topic_name, entry))
    {
        return E_DEFAULT;
    }

    entry->add_publisher(publisher);
    return E_SUCCESS;
}

int32_t topic_registry::register_subscriber(const char *topic_name, subscriber * subscriber)
{
    topic_entry *entry = NULL;
    if (false == m_entries.find((char *)topic_name, entry))
    {
        return E_DEFAULT;
    }

    entry->add_subscriber(subscriber);
    return E_SUCCESS;
}

void topic_registry::clear()
{
    string topic_name;
    topic_entry *entry = NULL;

    POSITION pos = m_entries.get_start_position();
    while (pos)
    {
        m_entries.get_next(pos, topic_name, entry);

        DELETE(entry);
    }

    m_entries.remove_all();
}

#endif


