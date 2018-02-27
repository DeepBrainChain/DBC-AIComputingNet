/******************************************************************************

                  ��Ȩ���� (C), , 

 ******************************************************************************
  �� �� ��   : test_attitude_node.h
  �� �� ��   : ����
  ��    ��   : Bruce Feng
  ��������   : 2017��4��17��
  ����޸�   :
  ��������   : ���ļ����������̬����ģ�����
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2017��4��17��
    ��    ��   : Bruce Feng
    �޸�����   : �����ļ�

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

    //ҵ����Ϣ������
    int32_t on_test_invoke_attitude(message_t *message)
    {
        SENSOR *sensor = (SENSOR *)message->body_bytes;

        return E_SUCCESS;
    }

};


#endif



