/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : test_service_manager.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年5月3日
  最近修改   :
  功能描述   : 本文件负责测试ServiceNodeManager
  函数列表   :
  修改历史   :
  1.日    期   : 2017年5月3日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _TEST_SERVICE_MANAGER_H_
#define _TEST_SERVICE_MANAGER_H_

#include "test_sensor_node.h"
#include "test_attitude_node.h"
#include "service_node_manager.h"

extern volatile U64 stk_sensor[1024 / 8];
extern volatile U64 stk_attitude[1204 / 8];

__BEGIN_DECLS

__task void test_service_node();

__END_DECLS

class TestServiceNodeManager : public ServiceNodeManager
{
public:

    TestServiceNodeManager() {}

    virtual int32_t create_service_nodes()
    {
        //create sensor node
        service_node_meta_data sensor_meta;    

        sensor_meta.task_priority = 0;
        sensor_meta.max_message_count = 3;
        sensor_meta.stack = (void *)stk_sensor;
        sensor_meta.stack_size = sizeof(stk_sensor);
        
        TestSensorNode *m_sensor_node = new TestSensorNode(&sensor_meta);
        m_sensor_node->init();
        m_sensor_node->start();

        os_dly_wait(3);

        //create attitude node
        service_node_meta_data attitude_meta;

        attitude_meta.task_priority = 0;
        attitude_meta.max_message_count = 3;
        attitude_meta.stack = (void *)stk_attitude;
        attitude_meta.stack_size = sizeof(stk_attitude);

        TestAttitudeNode *m_attitude_node = new TestAttitudeNode(&attitude_meta);
        m_attitude_node->init();
        m_attitude_node->start();

        return E_SUCCESS;
    }

    virtual int32_t destroy_service_nodes()
    {
        //不需要销毁节点
        return E_SUCCESS;
    }

private:

    TestSensorNode *m_sensor_node;
    TestAttitudeNode *m_attitude_node;
};

#endif


