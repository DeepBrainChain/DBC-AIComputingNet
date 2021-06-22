/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : os_math.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月3日
  最近修改   :
  功能描述   : 本文件负责数据计算
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月3日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _OS_MATH_H_
#define _OS_MATH_H_


#define ABS(x) ( (x)>0?(x):-(x) )
#define LIMIT( x,min,max ) ( (x) < (min)  ? (min) : ( (x) > (max) ? (max) : (x) ) )
#define _MIN(a, b) ((a) < (b) ? (a) : (b))
#define _MAX(a, b) ((a) > (b) ? (a) : (b))

#define my_pow(a) ((a)*(a))
#define safe_div(numerator,denominator,safe_value) ((denominator == 0) ? (safe_value) : ((numerator)/(denominator)))


float my_sqrt(float number);


#endif


