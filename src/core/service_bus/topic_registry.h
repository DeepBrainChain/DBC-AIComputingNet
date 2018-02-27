/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : topic_registry.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月20日
  最近修改   :
  功能描述   : topic registry which stores all the topics in service bus
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月20日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TOPIC_REGISTRY_H_
#define _TOPIC_REGISTRY_H_

#ifdef __RTX

#include "service_bus.h"
#include "topic_entry.h"
#include "map.h"


class __PRIVATE topic_registry
{
public:

    topic_registry() {}

    virtual ~topic_registry();

    virtual int32_t init();

public:

    topic_entry *get_topic_entry(const char * topic_name);

    virtual void register_topic_entry(const char *topic_name, topic_entry *entry);

    virtual topic_entry * create_topic_entry(const topic_meta_data *meta, uint32_t max_instance_count = 1);

public:

    list<publisher *> *get_publishers(const char * topic_name);

    list<subscriber *> *get_subscribers(const char * topic_name);

    int32_t register_publisher(const char *topic_name, publisher * publisher);

    int32_t register_subscriber(const char *topic_name, subscriber * subscriber);    

protected:

    void clear();

protected:

    MapStrToPtr<topic_entry> m_entries;

};

#endif

#endif



