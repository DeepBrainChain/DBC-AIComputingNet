/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : topic_entry.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年2月28日
  最近修改   :
  功能描述   : 本文件主要负责维护主题信息、发布者和订阅者以及主题数据。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年2月28日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TOPIC_ENTRY_H_
#define _TOPIC_ENTRY_H_

#ifdef __RTX

#include "service_bus.h"
#include "topic.h"
#include "list.h"
#include "topic_data_mailbox.h"


class publisher;
class subscriber;

class __PRIVATE topic_entry
{
public:

    topic_entry(const topic_meta_data *meta) : m_topic(meta) {}

    virtual ~topic_entry();

public:    

    topic *get_topic();

    uint32_t get_update_tick();

    int32_t send_to_subscribers(const void * topic_data);

public:    

    list<publisher *> &get_publisers();

    list<subscriber *> &get_subscribers();

    int32_t add_publisher(publisher * publisher);

    int32_t remove_publisher(publisher * publisher);

    int32_t add_subscriber(subscriber * subscriber);

    int32_t remove_subscriber(subscriber * subscriber);

    void remove_all_publishers();

    void remove_all_subscribers();

protected:

    topic m_topic;

    //topic_data_mailbox m_topic_mailbox;

    list<publisher *> m_publishers;             //是否需要加锁，先不加锁，这里外部调用不能随意释放

    list<subscriber *> m_subscribers;
};

#endif

#endif


