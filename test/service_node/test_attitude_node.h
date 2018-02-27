/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : test_attitude_node.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年4月17日
  最近修改   :
  功能描述   : 本文件负责测试姿态估计模块代码
  函数列表   :
  修改历史   :
  1.日    期   : 2017年4月17日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _TEST_ATTITUDE_NODE_H_
#define _TEST_ATTITUDE_NODE_H_


#include "service_node.h"
#include "test_topic_attitude.h"


class TestAttitudeNode : public ServiceNode
{
public:

    TestAttitudeNode(service_node_meta_data *meta) : ServiceNode(meta)
    {

    }

    virtual int32_t on_invoke(message_t *message)
    {
        switch (message->header.message_id)
        {
            case TEST_MESSAGE_ID_ATTITUDE:
                on_test_invoke_attitude(message);
                break;

            default:
                break;
        }

        return E_SUCCESS;        
    }

    //业务消息处理函数
    int32_t on_test_invoke_attitude(message_t *message)
    {
        SENSOR *sensor = (SENSOR *)message->body_bytes;

        return E_SUCCESS;
    }

};


#endif



