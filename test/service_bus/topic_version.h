/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : topic_version.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年3月25日
  最近修改   :
  功能描述   : 版本信息主题-发布到服务总线
  函数列表   :
  修改历史   :
  1.日    期   : 2017年3月25日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TOPIC_VERSION_H_
#define _TOPIC_VERSION_H_


#include "service_bus.h"
#include "service_message.h"

#define MESSAGE_VERSION            0x00000001

//定义发布到总线的主题结构体

struct version_t
{
    uint32_t version_tick;
};

//声明总线消息
TOPIC_DECLARE(version);

#endif

