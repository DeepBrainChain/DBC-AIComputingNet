/******************************************************************************

                  ��Ȩ���� (C), , 

 ******************************************************************************
  �� �� ��   : test_topic_attitude.h
  �� �� ��   : ����
  ��    ��   : Bruce Feng
  ��������   : 2017��4��17��
  ����޸�   :
  ��������   : ���ļ�����ģ����̬���ݵĽṹ��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2017��4��17��
    ��    ��   : Bruce Feng
    �޸�����   : �����ļ�

******************************************************************************/


#ifndef _TEST_TOPIC_ATTITUDE_H_
#define _TEST_TOPIC_ATTITUDE_H_


#define TEST_MESSAGE_ID_ATTITUDE        0x00000001


typedef struct 
{
    float p;
    float q;
    float r;

    float acx;		//������������ļ��ٶȼ�ֵ
    float acy;
    float acz;

    float acx_r;    //ԭʼ���ٶȼ�ֵ
    float acy_r;
    float acz_r;

} SENSOR;


#endif



