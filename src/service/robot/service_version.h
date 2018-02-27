/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : service_version.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年10月28日
  最近修改   :
  功能描述   : 本文件负责定义所有设备版本相关定义
  函数列表   :
  修改历史   :
  1.日    期   : 2017年10月28日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _SERVICE_VERSION_H_
#define _SERVICE_VERSION_H_


#ifdef FC_I_V1

#include "server.h"
#include "FC_I_V1_device_manager.h"
#include "FC_I_V1_Conf.h"

extern Server *g_server;

#define DEVICE_MANAGER ((FC_I_V1_DeviceManager *)(g_server->get_device_manager()))
#define MPU6050_DEVICE    (DEVICE_MANAGER->get_mpu6050())
#define AK8975_DEVICE (DEVICE_MANAGER->get_ak8975())
#define I2C_DEVICE    (DEVICE_MANAGER->get_i2c())
#define ULTRASONIC_DEVICE (DEVICE_MANAGER->get_ultrasonic())
#define PWM_DEVICE (DEVICE_MANAGER->get_pwm())
#define USB_HID_DEVICE (DEVICE_MANAGER->get_usb_hid())
#define MS5611_DEVICE (DEVICE_MANAGER->get_ms5611())
#define LED_DEVICE (DEVICE_MANAGER->get_led())
#define USART_DEVICE (DEVICE_MANAGER->get_usart())


#define GET_CONF    ((FC_I_V1_Conf *)g_server->get_conf())
#define GET_ALARM (g_server->get_alarm())





#endif


#endif
