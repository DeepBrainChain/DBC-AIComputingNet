/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : topic_data_mailbox.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年2月28日
  最近修改   :
  功能描述   : 本文件负责存储该主题的数据，参考邮箱方式，送信和取信到邮箱。
               邮箱尽量采用无锁模式实现。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年2月28日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TOPIC_DATA_MAILBOX_H_
#define TOPIC_DATA_MAILBOX_H_

#ifdef __RTX

#include "common.h"
#include "mutex.h"

#ifdef __RTX
#define DEFAULT_MAIL_COUNT      3
#else
#define DEFAULT_MAIL_COUNT      1000
#endif

#define DEFAULT_CUR_MAIL_COUNT      0

class __PRIVATE topic_data_mailbox
{
public:

    topic_data_mailbox(uint32_t mail_size, uint32_t mail_count = DEFAULT_MAIL_COUNT)
    : m_mailboxes(NULL), m_mail_size(mail_size), m_mail_count(mail_count), m_update_tick(0)
    {}

    virtual ~topic_data_mailbox();

    virtual void init();

    virtual void clear();

    //publisher send to mailbox
    virtual int32_t send(const void *topic_data);

    //subscriber recv from mailbox
    virtual int32_t recv(volatile uint32_t &subscriber_tick, void *topic_buf);

    uint32_t get_update_tick() {return m_update_tick;}

    int32_t set_mail_count(uint32_t max_mail_count);

    uint32_t get_mail_count() {return m_mail_count;}

protected:

    uint32_t get_cur_mail() {return m_update_tick % m_mail_count;}

    uint32_t alloc_mailboxes();

protected:

    uint8_t **m_mailboxes;    

    uint32_t m_mail_size;

    uint32_t m_mail_count;

    volatile uint32_t m_update_tick;    

};

#endif

#endif


