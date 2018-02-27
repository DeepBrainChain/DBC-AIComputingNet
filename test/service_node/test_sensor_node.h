/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : test_sensor_node.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年4月18日
  最近修改   :
  功能描述   : 本文件负责模拟传感器节点。
  函数列表   :
  修改历史   :
  1.日    期   : 2017年4月18日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _TEST_SENSOR_NODE_H_
#define _TEST_SENSOR_NODE_H_


#include "service_bus.h"
#include "service_node.h"
#include "test_topic_attitude.h"



class TestSensorNode : public ServiceNode
{
public:

    TestSensorNode(service_node_meta_data *meta) : ServiceNode(meta)
    {
        
    }

    virtual int32_t run()
    {
        message_t attitude_message;
        memset(&attitude_message, 0x00, sizeof(message_t));
        
        attitude_message.header.message_id = TEST_MESSAGE_ID_ATTITUDE;
        SENSOR *sensor = (SENSOR *)attitude_message.body_bytes;
        
        //register topic
        PUBLISHER_HANDLE handle = register_topic(TOPIC_ID(message));
        
        while(true)
        {
            os_dly_wait(1440);
            
            //publish data
            publish_topic_data(handle, &attitude_message);
        
            //update message
            sensor->acx += 1.0;
            sensor->acy += 1.0;
            sensor->acz += 1.0;
            
            sensor->acx_r += 1.0;
            sensor->acy_r += 1.0;
            sensor->acz_r += 1.0;
        
            sensor->p += 1.0;
            sensor->q += 1.0;
            sensor->r += 1.0;
        }        
    }
    
};

#endif



