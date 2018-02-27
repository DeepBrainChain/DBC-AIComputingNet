/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : ctrl_def.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月21日
  最近修改   :
  功能描述   : 本文件负责控制基本定义
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月21日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _CTRL_DEF_H_
#define _CTRL_DEF_H_

#include "param_def.h"

enum 
{
    PIDROLL,
    PIDPITCH,
    PIDYAW,
    PID4,
    PID5,
    PID6,
    PIDITEMS
};

typedef struct
{
    xyz_f_t err;
    xyz_f_t err_old;
    xyz_f_t err_i;
    xyz_f_t eliminate_I;
    xyz_f_t err_d;
    xyz_f_t damp;
    xyz_f_t out;
    pid_t  PID[PIDITEMS];
    xyz_f_t err_weight;
    float FB;
}ctrl_t;

extern ctrl_t ctrl_1;
extern ctrl_t ctrl_2;


#endif


