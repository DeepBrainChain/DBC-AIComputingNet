/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : MemsService
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月26日
  最近修改   :
  功能描述   : 本文件负责MEMS定时采集服务
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月26日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _MEMS_SERVICE_H_
#define _MEMS_SERVICE_H_


#include "common.h"
#include "service_node.h"


#define MEMS_TIMER_NAME             0x00000001
#define MEMS_PERIOD_MS                  2


class MemsService : public ServiceNode
{
public:

    MemsService(service_node_meta_data *meta);

    virtual ~MemsService();

    virtual int32_t on_time_out(Timer *timer);

protected:

    //业务层override
    
    virtual int32_t service_init();

    virtual int32_t service_exit();

    virtual int32_t on_invoke(message_t *message);

    virtual int32_t on_read_mems();

    float cal_cycle();

protected:

    uint32_t m_mems_timer_id;

    float m_cur_read_us, m_old_read_us;

    float m_cycle;              //us / 1000000.0f
    
};


#endif


