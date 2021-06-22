/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : visibility.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月25日
  最近修改   :
  功能描述   : visibility macro definition
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月25日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _VISIBILITY_H_
#define _VISIBILITY_H_

#ifdef __EXPORT
#undef __EXPORT
#endif
#define __EXPORT __attribute__ ((visibility ("default")))

#ifdef __PRIVATE
#undef __PRIVATE
#endif
#define __PRIVATE __attribute__ ((visibility ("hidden")))

#ifdef __cplusplus
#define __BEGIN_DECLS               extern "C" {
#define __END_DECLS                     }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#endif
