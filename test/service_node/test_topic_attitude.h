/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : test_topic_attitude.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年4月17日
  最近修改   :
  功能描述   : 本文件定义模拟姿态数据的结构体
  函数列表   :
  修改历史   :
  1.日    期   : 2017年4月17日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _TEST_TOPIC_ATTITUDE_H_
#define _TEST_TOPIC_ATTITUDE_H_


#define TEST_MESSAGE_ID_ATTITUDE        0x00000001


typedef struct 
{
    float p;
    float q;
    float r;

    float acx;		//角速率修正后的加速度计值
    float acy;
    float acz;

    float acx_r;    //原始加速度计值
    float acy_r;
    float acz_r;

} SENSOR;


#endif



