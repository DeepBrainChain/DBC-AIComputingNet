/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : subscriber.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年3月5日
  最近修改   :
  功能描述   : 本文件负责订阅者职责，用于维护订阅者相关信息。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年3月5日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _SUBSCRIBER_H_
#define _SUBSCRIBER_H_

#ifdef __RTX

#include "common.h"
#include "topic_entry.h"
#include "sem.h"


class topic_entry;

class __PRIVATE subscriber
{
public:

    subscriber(topic_entry *entry) : m_entry(entry), m_topic_mailbox(entry->get_topic()->get_topic_size()), m_local_update_tick(0)  {m_topic_mailbox.init();}

    ~subscriber() {}

public:

    topic_entry *get_topic_entry() {return m_entry;}    

    void set_topic_entry(topic_entry *entry) {m_entry = entry;}

    void notify_topic() {m_topic_sem.send();}
    
    int32_t wait_topic(uint16_t timeout = 0xFFFF) {return m_topic_sem.wait(timeout);}

    int32_t recv(void *topic_buf);

    int32_t send(const void *topic_data);

    bool is_updated();

    int32_t set_mail_count(uint32_t max_mail_count);

protected:

    Mutex m_mutex;
    
    topic_entry *m_entry;

    Semaphore m_topic_sem;

    topic_data_mailbox m_topic_mailbox;

    volatile uint32_t m_local_update_tick;
};

#endif

#endif


