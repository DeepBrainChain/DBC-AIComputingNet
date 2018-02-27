/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : delay.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年4月18日
  最近修改   :
  功能描述   : 本文件提供延时函数
  函数列表   :
  修改历史   :
  1.日    期   : 2017年4月18日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _TEST_DELAY_H_
#define _TEST_DELAY_H_


#include "service_bus.h"


__BEGIN_DECLS

void sys_tick_init(uint32_t hz_per_second);

void delay_us(volatile uint32_t time);

void time_decrement(void);

__END_DECLS

#endif



