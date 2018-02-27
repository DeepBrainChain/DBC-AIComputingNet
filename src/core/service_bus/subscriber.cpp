#ifdef __RTX

#include "subscriber.h"


int32_t subscriber::send(const void *topic_data) 
{
    AutoMutex<Mutex> auto_lock(&m_mutex);
    return m_topic_mailbox.send(topic_data);
}

int32_t subscriber::recv(void *topic_buf) 
{
    AutoMutex<Mutex> auto_lock(&m_mutex);
    return  m_topic_mailbox.recv(m_local_update_tick, topic_buf);
}

bool subscriber::is_updated() 
{
    return m_local_update_tick != m_topic_mailbox.get_update_tick();
}


int32_t subscriber::set_mail_count(uint32_t max_mail_count)
{
    return m_topic_mailbox.set_mail_count(max_mail_count);
}

#endif


