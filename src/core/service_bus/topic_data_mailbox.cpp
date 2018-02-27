#ifdef __RTX

#include "topic_data_mailbox.h"

#include <string.h>
#include <stdlib.h>

#include "common.h"


topic_data_mailbox::~topic_data_mailbox()
{
    clear();
}

void topic_data_mailbox::init()
{
    //alloc mem
    alloc_mailboxes();
}

int32_t topic_data_mailbox::send(const void *topic_data)
{
    if (NULL == topic_data)
    {
        return E_BAD_PARAM;
    }

    memcpy(m_mailboxes[m_update_tick % m_mail_count], (void *)topic_data, m_mail_size);
    m_update_tick++;

    return E_SUCCESS;
}

int32_t topic_data_mailbox::recv(volatile uint32_t &subscriber_tick, void *topic_buf)
{
    if (m_update_tick > subscriber_tick + m_mail_count)
    {
        subscriber_tick = m_update_tick - m_mail_count;             //lost messages
    }
    else if (subscriber_tick >= m_update_tick)
    {
        subscriber_tick--;
    }

    if (NULL != topic_buf)
    {
        memcpy(topic_buf, m_mailboxes[subscriber_tick % m_mail_count], m_mail_size);        //unknown size, dangerous
        if (subscriber_tick < m_update_tick)
        {
            subscriber_tick++;
        }
    }
    
    return E_SUCCESS;
}

void topic_data_mailbox::clear()
{
    if (NULL == m_mailboxes)
    {
        return;
    }

    for (int i = 0; i < m_mail_count; i++)
    {
        free(m_mailboxes[i]);
        m_mailboxes[i] = NULL;
    }

    free(m_mailboxes);
    m_mailboxes = NULL;
}

uint32_t topic_data_mailbox::alloc_mailboxes()
{
    //alloc array
    uint32_t mailboxes_size = sizeof(uint8_t *) * m_mail_count;
    m_mailboxes = (uint8_t **)malloc(mailboxes_size);
    memset(m_mailboxes, 0x00, mailboxes_size);

    for (int i = 0; i < m_mail_count; i++)
    {
        m_mailboxes[i] = (uint8_t *)malloc(m_mail_size);
        memset(m_mailboxes[i], 0x00, m_mail_size);
    }

    return E_SUCCESS;
}

int32_t topic_data_mailbox::set_mail_count(uint32_t max_mail_count)
{
    if (m_mail_count == max_mail_count)
    {
        return E_SUCCESS;
    }
    else
    {
        clear();
        
        m_mail_count = max_mail_count;
        
        return alloc_mailboxes();
    }
}

#endif





