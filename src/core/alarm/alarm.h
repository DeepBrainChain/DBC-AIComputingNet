/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : alarm.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月15日
  最近修改   :
  功能描述   : 本文件负责告警管理
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月15日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _ALARM_H_
#define _ALARM_H_

#include "common.h"
#include "alarm_def.h"


class alarm
{
public:

    //Alarm();
    virtual void alarm_debug(uint32_t type, const char * psz = NULL);

    virtual void alarm_info(uint32_t type, const char * psz = NULL);

    virtual void alarm_warning(uint32_t type, const char * psz = NULL);
    
    virtual void alarm_critical(uint32_t type, const char * psz = NULL);

protected:

    virtual void alarm_info_alive();

    virtual void alarm_critical_mpu_err();

    virtual void alarm_critical_mag_err();

    virtual void alarm_critical_ms5611_err();
    

};



#endif


