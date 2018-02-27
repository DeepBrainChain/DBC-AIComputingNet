/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : magnetometer_service.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月26日
  最近修改   :
  功能描述   : 本文件负责磁力计采集服务
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月26日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _MAGNETOMETER_SERVICE_H_
#define _MAGNETOMETER_SERVICE_H_


#include "service_node.h"

#define MAG_TIMER_NAME             0x00000001
#define MAG_PERIOD_MS                 5



class MagnetometerService : public ServiceNode
{
public:

    MagnetometerService(service_node_meta_data *meta);

    virtual ~MagnetometerService();

    virtual int32_t on_time_out(Timer *timer);

protected:

    //业务层override
    
    virtual int32_t service_init();

    virtual int32_t service_exit();

    virtual int32_t on_invoke(message_t *message);

    virtual int32_t on_read_mag();

protected:

    uint32_t m_mag_timer_id;    

};


#endif




