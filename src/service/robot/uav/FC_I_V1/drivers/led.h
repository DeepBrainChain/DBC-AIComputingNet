/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : led.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月3日
  最近修改   :
  功能描述   : 本文件负责LED等管理
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月3日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _LED_H_
#define _LED_H_


#include "common.h"
#include "stm32f4xx_rcc.h"


/***************LED GPIO定义******************/
#define OS_RCC_LED     RCC_AHB1Periph_GPIOE

#define OS_GPIO_LED    GPIOE

#define OS_Pin_LED1    GPIO_Pin_3
#define OS_Pin_LED2    GPIO_Pin_2
#define OS_Pin_LED3    GPIO_Pin_1
#define OS_Pin_LED4    GPIO_Pin_0
/*********************************************/



class LED
{
public:

    LED() {}

    virtual ~LED() {}

    virtual int32_t init();
    
    //LED ON
    void LED1_ON() {OS_GPIO_LED->BSRRH = OS_Pin_LED1;}      //BSRR高字节
    
    void LED2_ON() {OS_GPIO_LED->BSRRH = OS_Pin_LED2;}
    
    void LED3_ON() {OS_GPIO_LED->BSRRH = OS_Pin_LED2;}
    
    //LED OFF
    void LED1_OFF() {OS_GPIO_LED->BSRRL = OS_Pin_LED1;}     //BSRR低字节

    void LED2_OFF() {OS_GPIO_LED->BSRRL = OS_Pin_LED2;}

    void LED3_OFF() {OS_GPIO_LED->BSRRL = OS_Pin_LED4;}

};


#endif


