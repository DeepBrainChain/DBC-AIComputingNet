/******************** (C) COPYRIGHT 2017 Robot OS -- Bruce Feng  ***********************
 * 文件名  ：std_types.h
 * 描述    ：定义跨平台使用的类型定义
 * 实验平台：
 * 标准库  ：STM32F10x_StdPeriph_Driver V3.5.0
 * 作者    ：Bruce Feng
**********************************************************************************/

#ifndef _STD_TYPES_H_
#define _STD_TYPES_H_


/****************************************************************************
 * Basic Types
 ****************************************************************************/

#ifdef USE_MATRIX_DEFINE_TYPE

typedef signed char        int8_t;
typedef unsigned char   uint8_t;

typedef signed short      int16_t;
typedef unsigned short  uint16_t;

typedef signed int          int32_t;
typedef unsigned int    uint32_t;    
typedef   signed       __int64 int64_t;
typedef unsigned       __int64 uint64_t;

#ifndef NULL
 #ifdef __cplusplus
  #define NULL          0
 #else
  #define NULL          ((void *) 0)
 #endif
#endif

#ifndef EOF
 #define EOF            (-1)
#endif

#ifndef __size_t
#define __size_t       1
typedef unsigned int   size_t;
#endif

typedef void * POSITION;

#else

#include <stdint.h>

#endif /*USE_MATRIX_DEFINE_TYPE*/

#endif

/******************* (C) COPYRIGHT 2017 Robot OS -- Bruce Feng *****END OF FILE****/
