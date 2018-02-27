/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : FC_I_V1_Initiator.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月2日
  最近修改   :
  功能描述   : 本文件负责FC_I_V1 入门级V1版本飞控
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月2日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _FC_I_V1_H_
#define _FC_I_V1_H_

#include "common.h"
#include "server_initiator.h"


class FC_I_V1_Initiator : public ServerInitiator
{
public:

    FC_I_V1_Initiator() {}

    virtual ~FC_I_V1_Initiator() {}

    virtual int32_t init();

    virtual int32_t exit();


protected:

    virtual int32_t init_conf();

    virtual int32_t init_alarm();

    virtual int32_t init_device_manager();

    virtual int32_t init_nvic();

    virtual int32_t init_os_clk();

    virtual int32_t init_i2c();

    virtual int32_t init_pwm();

    virtual int32_t init_usb_hid();

    virtual int32_t init_ms5611();

    virtual int32_t init_mpu6050();

    virtual int32_t init_ak8975();

    virtual int32_t init_led();

    virtual int32_t init_usart();

    virtual int32_t init_ultrasonic();

    virtual int32_t init_service();
    
    /*
        
    virtual int32_t init_rcc();

    virtual int32_t init_gpio();
    

    virtual int32_t init_adc();
    

    virtual int32_t init_servo();


    virtual int32_t init_usart_osd();

    virtual int32_t init_receiver();

    virtual int32_t init_rpm();

    virtual int32_t init_timer();

    virtual int32_t init_data_link();

    virtual int32_t init_gps();
    
    */

protected:

    int32_t check_sys_clk();
    
};


#endif


