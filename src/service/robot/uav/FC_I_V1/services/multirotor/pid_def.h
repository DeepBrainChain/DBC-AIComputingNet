/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : pid_def.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月21日
  最近修改   :
  功能描述   : 本文件负责PID结构体定义
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月21日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _PID_DEF_H_
#define _PID_DEF_H_

typedef struct
{
    float fb_lim;                   //反馈限幅
    float value_old;            //历史数值
    float value;                    //估计输出(实际)
    float ut;                           //阶跃响应上升时间
    float fb_t;                         //反馈滞后时间
    
    float ct_out;                 //控制器的输出量
    float feed_back;        //反馈值
    float fore_feed_back;   //估计的反馈值
}forecast_t;


typedef struct
{
    float kp;                       //比例系数
    float ki;                       //积分系数
    float kd;                       //微分系数
    float k_pre_d;          //previous_d 微分先行
    float inc_hz;               //不完全微分低通系数
    float k_inc_d_norm;     //Incomplete 不完全微分归一 (0,1)
    float k_ff;                     //前馈
}_PID_arg_st;


typedef struct
{
    float err;
    float err_old;
    float feedback_old;
    float feedback_d;
    float err_d;
    float err_d_lpf;
    float err_i;
    float ff;
    float pre_d;

}_PID_val_st;



#endif


