/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : FC_I_V1_device_manager.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月14日
  最近修改   :
  功能描述   : 本文件负责FC_I V1版本飞控设备管理器, 作为设备容器
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月14日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _FC_I_VI_DEVICE_MANAGER_H_
#define _FC_I_VI_DEVICE_MANAGER_H_


#include "device_manager.h"
#include "os_i2c.h"
#include "pwm.h"
#include "led.h"
#include "usart.h"
#include "os_usb_hid.h"
#include "ms5611.h"
#include "mpu6050.h"
#include "ak8975.h"
#include "ultrasonic.h"


class FC_I_V1_DeviceManager : public DeviceManager
{

public:

    FC_I_V1_DeviceManager();

    void create_device();

    I2C *get_i2c();

    PWM *get_pwm();

    LED *get_led();

    USART *get_usart();

    USBHID *get_usb_hid();

    MPU6050 *get_mpu6050();    

    MS5611 *get_ms5611();

    AK8975 *get_ak8975();    

    Ultrasonic *get_ultrasonic();

protected:

    I2C *m_i2c;

    PWM *m_pwm;

    LED *m_led;

    USART *m_usart;

    MS5611 *m_ms5611;

    AK8975 *m_ak8975;

    MPU6050 *m_mpu6050;

    USBHID *m_usb_hid;

    Ultrasonic *m_ultrasonic;

};


#endif



