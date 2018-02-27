/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : topic.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月20日
  最近修改   :
  功能描述   : 定义要发布的主题，包括：主题名称、大小、主题内容等
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月20日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TOPIC_H_
#define _TOPIC_H_

#ifdef __RTX

#include "service_bus.h"


class __PRIVATE topic
{
public:

    topic(const topic_meta_data *meta) : m_meta_data(*meta) {};

    uint32_t get_topic_size() const;

    const char * get_topic_name() const;

    const char * get_topic_fields() const;

    const topic_meta_data &get_meta_data() const;    

private:

    //meta data definition
    const topic_meta_data m_meta_data;
    
};

#endif

#endif

/******************* (C) COPYRIGHT 2017 Robot OS -- Bruce Feng *****END OF FILE****/
