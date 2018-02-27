/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : pwm.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月3日
  最近修改   :
  功能描述   : 本文件负责PWM驱动
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月3日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _PWM_H_
#define _PWM_H_

#include "common.h"
#include "os_math.h"


#define INIT_PULSE                   4000         //u16(1000/0.25)
#define ACCURACY                   10000       //u16(2500/0.25)        //ARR 自动重装载寄存器的计数值

#define PWM_RADIO               4               //(8000 - 4000)/1000.0

#define MIN_MOTORS            1
#define MAX_MOTORS           8
#define DEFAULT_MOTORS   4


__BEGIN_DECLS

void _TIM3_IRQHandler(void);
void _TIM4_IRQHandler(void);

__END_DECLS


//系统使用
//通用定时器TIM3, TIM4 
//TIM3: GPIOC Pin6|Pin7, GPIOB Pin0|Pin1, 
//TIM4: GPIOD GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15 作为遥控器PWM IN编码输入;

//高级定时器TIM1, TIM8, 通用定时器TIM5
//TIM5: GPIOA GPIOA GPIO_Pin_2|GPIO_Pin_3, 
//TIM1: GPIOE GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14
//TIM8: GPIOC GPIO_Pin_8 | GPIO_Pin_9 TIM8 作为电调输出; 

class PWM
{
public:

    //后面motor_count修改为配置
    PWM(int16_t min, int16_t max, uint8_t motor_count) : m_min(min), m_max(max), m_motor_count(motor_count) {}

    virtual ~PWM() {}

    virtual int32_t init();

    virtual int32_t set_pwm(int16_t pwm[MAX_MOTORS]);

    virtual uint8_t get_motor_count() {return m_motor_count;}

    virtual void   set_motor_count(uint8_t motor_count) {m_motor_count = LIMIT(motor_count, MIN_MOTORS, MAX_MOTORS);}

protected:

    //遥控器PWM输入--遥控器接收机
    int32_t init_in();

    //电调PWM输出
    int32_t init_out(uint16_t hz);

protected:

    int16_t m_min;

    int16_t m_max;

    uint8_t m_motor_count;

    uint8_t m_ch_out_mapping[MAX_MOTORS] = {0,1,2,3,4,5,6,7};
    
};

#endif


