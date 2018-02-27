/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : sem.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年3月5日
  最近修改   :
  功能描述   : 本文件负责定义RTX平台的信号量类。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年3月5日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _RTX_SEM_H_
#define _RTX_SEM_H_


#ifdef __RTX

#include <rtl.h>
#include "sem_def.h"

class Semaphore
{
public:

    Semaphore(uint16_t token_count = 0) {os_sem_init(m_sem, token_count);}

    ~Semaphore() {}

    int32_t send()
    {
        return OS_R_OK == os_sem_send(m_sem) ? E_SUCCESS : E_DEFAULT;
    }

    int32_t wait(uint16_t timeout)
    {
        return OS_R_TMO != os_sem_wait(m_sem, timeout) ? SEM_OK : SEM_TIMEOUT;
    }

private:

    OS_SEM m_sem;

};

#endif

#endif


