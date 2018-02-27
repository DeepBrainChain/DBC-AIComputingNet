/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : i2c.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月3日
  最近修改   :
  功能描述   : 本文件负责软件模拟的I2C驱动
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月3日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _I2C_H_
#define _I2C_H_


#include "common.h"


#define OS_GPIO_I2C    GPIOB

#define I2C_Pin_SCL      GPIO_Pin_6
#define I2C_Pin_SDA     GPIO_Pin_7

#define OS_RCC_I2C     RCC_AHB1Periph_GPIOB

#define SCL_H                 OS_GPIO_I2C->BSRRL = I2C_Pin_SCL
#define SCL_L                  OS_GPIO_I2C->BSRRH = I2C_Pin_SCL

#define SDA_H               OS_GPIO_I2C->BSRRL = I2C_Pin_SDA
#define SDA_L                OS_GPIO_I2C->BSRRH = I2C_Pin_SDA

#define SCL_read          OS_GPIO_I2C->IDR   & I2C_Pin_SCL
#define SDA_read         OS_GPIO_I2C->IDR   & I2C_Pin_SDA

#define MAX_WAIT_ERR_TIMES   50

#define I2C_ACK            0x01
#define I2C_NACK        0x00

#define DELAY_WAIT_TICK 15


class I2C
{
public:

    I2C() : m_fast_mode(false) {}

    virtual int32_t init();

    virtual int32_t start();

    virtual int32_t stop();

    virtual bool is_fast_mode() {return m_fast_mode;}

    virtual void set_fast_mode(bool fast_mode) {m_fast_mode = fast_mode;}

protected:

    virtual void ack();

    virtual void nack();

    virtual void delay();

    virtual int32_t wait_ack();

protected:

    virtual uint8_t read_byte(uint8_t is_ack);

    virtual int32_t send_byte(uint8_t SendByte);

public:

    virtual int32_t read_1byte(uint8_t slave_address,uint8_t reg_address,uint8_t *reg_data);

    virtual int32_t write_1byte(uint8_t slave_address,uint8_t reg_address,uint8_t reg_data);

    virtual int32_t write_1bit(uint8_t dev, uint8_t reg, uint8_t bit_num, uint8_t data);

    virtual int32_t write_bits(uint8_t dev, uint8_t reg, uint8_t bit_start, uint8_t length, uint8_t data);

    virtual int32_t read_nbytes(uint8_t slave_address, uint8_t reg_address, uint8_t len, uint8_t *buf);

    virtual int32_t write_nbytes(uint8_t slave_address, uint8_t reg_address, uint8_t len, uint8_t *buf);

protected:

    volatile bool m_fast_mode;
    
};


#endif



