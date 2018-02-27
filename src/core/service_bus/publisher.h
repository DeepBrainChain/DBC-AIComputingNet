/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : publisher.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年3月6日
  最近修改   :
  功能描述   : 本文件负责发布者职责，映射到发布者实体角色。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年3月6日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _PUBLISHER_H_
#define _PUBLISHER_H_

#ifdef __RTX

#include "common.h"


class topic_entry;

class __PRIVATE publisher
{
public:

    publisher(topic_entry *entry) : m_entry(entry) {}

    ~publisher() {m_entry = NULL;}

    topic_entry *get_topic_entry() {return m_entry;}    

    void set_topic_entry(topic_entry *entry) {m_entry = entry;}

protected:

    topic_entry *m_entry;

};

#endif

#endif

