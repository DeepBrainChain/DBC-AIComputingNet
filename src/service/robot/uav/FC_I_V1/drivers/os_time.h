/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : time.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月4日
  最近修改   :
  功能描述   : 本文件负责时间处理
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月4日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _OS_TIME_H_
#define _OS_TIME_H_

#include "common.h"


#define TICK_PER_SECOND         1000                                                                        //每秒 钟的频率  1000 tick, 每毫秒1个tick
#define US_PER_TICK                   (1000000 / TICK_PER_SECOND)                   //每个Tick多少微秒


#define DELAY_MS(MS_TIME)       OSTime::delay_ms(MS_TIME);
#define DELAY_US(US_TIME)         OSTime::delay_us(US_TIME);


class OSTime
{
public:

    //初始化时钟
    static int32_t init();
    
    static uint32_t get_os_time_us();
    
    static void delay_us(uint32_t us);

    static void delay_ms(uint32_t ms);

    static void update_os_time();

    static uint64_t get_sys_tick_up_tme();

public:

    static volatile int m_time_day;

    static volatile int m_time_hour;

    static volatile int m_time_min;

    static volatile int m_time_second;

    static volatile int m_time_ms;

    static volatile uint64_t SYS_TICK_UP_TME;
    
};

#endif

