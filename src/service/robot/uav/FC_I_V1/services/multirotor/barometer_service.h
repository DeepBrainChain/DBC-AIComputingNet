/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : barometer_service.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月26日
  最近修改   :
  功能描述   : 本文件负责气压计读取职责
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月26日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _BAROMETER_SERVICE_H_
#define _BAROMETER_SERVICE_H_


#include "service_node.h"


#define BARO_TIMER_NAME             0x00000001
#define BARO_PERIOD_MS                 20


class BarometerService : public ServiceNode
{
public:

    BarometerService(service_node_meta_data *meta);

    virtual ~BarometerService();

    virtual int32_t on_time_out(Timer *timer);

protected:

    //业务层override
    
    virtual int32_t service_init();

    virtual int32_t service_exit();

    virtual int32_t on_invoke(message_t *message);

    virtual int32_t on_read_baro();

protected:

    uint32_t m_baro_timer_id;

};


#endif


