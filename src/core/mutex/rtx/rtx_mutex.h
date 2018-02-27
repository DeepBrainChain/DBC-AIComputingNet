/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : rtx_mutex.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年3月7日
  最近修改   :
  功能描述   : 本文件负责RTX操作系统下的互斥量。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年3月7日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _RTX_MUTEX_H_
#define _RTX_MUTEX_H_

#include <rtl.h>

class NullMutex
{
public:

    NullMutex() {}

    ~NullMutex() {}

    bool lock(uint16_t timeout = 0xFFFF) {return true;}

    bool unlock() {return true;}
};

class Mutex
{
public:

    Mutex() {os_mut_init(m_mutex);}

    ~Mutex() {}

    bool lock(uint16_t timeout = 0xFFFF) {return (OS_R_TMO == os_mut_wait(m_mutex, timeout)) ? false : true;}

    bool unlock() {return OS_R_OK == os_mut_release(m_mutex) ? true : false;}

protected:

    OS_MUT m_mutex;

};

template<class T>
class AutoMutex
{
public:

    AutoMutex(T * mutex) : m_mutex(mutex) {if (m_mutex) m_mutex->lock();}

    ~AutoMutex() {if (m_mutex) m_mutex->unlock();}

protected:

    T * m_mutex;

};

#endif


