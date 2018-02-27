/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : param_def.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月24日
  最近修改   :
  功能描述   : 本文件负责定义基础的业务参数结构体定义
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月24日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _PARAM_H_
#define _PARAM_H_

#include "common.h"

typedef struct 
{
    int16_t x;
    int16_t y;
    int16_t z;
}xyz_i16_t;

typedef struct 
{
  float x;
    float y;
    float z;
}xyz_f_t;

typedef struct
{
    float kp;
    float kd;
    float ki;
    float kdamp;
}pid_t;

typedef union
{
    uint8_t raw_data[64];
    
    struct
    {
        xyz_f_t Accel;
        xyz_f_t Gyro;
        xyz_f_t Mag;
        xyz_f_t vec_3d_cali;
        uint32_t mpu_flag;
        float Acc_Temperature;
        float Gyro_Temperature;
        
    }Offset;
    
}sensor_setup_t;                //__attribute__((packed))

typedef  struct
{
    pid_t roll;
    pid_t pitch;
    pid_t yaw;
}pid_group_t;

typedef union
{
    uint8_t raw_data[192];
 
    struct
    {
        pid_group_t ctrl1;
        pid_group_t ctrl2;
        
        /////////////////////
        pid_t hc_sp;
        pid_t hc_height;
        pid_t ctrl3;
        pid_t ctrl4;       
    }groups;

}pid_setup_t;


#define SENSOR_SETUP_CONF           0x00000001
#define PID_SETUP_CONF                    0x00000002


#endif
