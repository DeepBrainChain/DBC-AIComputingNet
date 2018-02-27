/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : ultrasonic.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月10日
  最近修改   :
  功能描述   : 本文件负责超声波驱动
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月10日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _ULTRASONIC_H_
#define _ULTRASONIC_H_


#include "common.h"


#define ULTRASONIC_TIME_OUT                                         0
#define ULTRASONIC_MEASURE_OK                                   1
#define ULTRASONIC_MEASURE_BEYOND_RANGE       2


typedef struct
{
    int32_t height;                                 //cm
    
    float relative_height;    
    float h_delta;
    float h_dt;
    
    uint8_t measure_ok;
    uint8_t measure_ot_cnt;
    
}height_t;


class Ultrasonic
{
public:
    
    virtual int32_t init();

    virtual int32_t read();
    
    virtual int32_t get(uint8_t com_data);
    
protected:
    
    int8_t m_ultra_start_f;
    
    uint16_t m_ultra_distance_old;
    
    height_t m_ultra;
};

#endif



