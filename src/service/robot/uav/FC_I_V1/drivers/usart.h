/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : 本文件负责USART驱动
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月9日
  最近修改   :
  功能描述   : 
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月9日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _USART_H_
#define _USART_H_

#include "common.h"


#define DEFAULT_BAUD_RATE       500000


__BEGIN_DECLS

extern __EXPORT void Usart2_IRQ(void);
extern __EXPORT void Uart4_IRQ(void);
extern __EXPORT void Uart5_IRQ(void);

__END_DECLS;


class USART
{
public:

    virtual int32_t init();

    virtual int32_t send_usart2(unsigned char *DataToSend, uint8_t data_num);

    virtual int32_t send_usart4(unsigned char *DataToSend, uint8_t data_num);

    virtual int32_t send_usart5(unsigned char *DataToSend, uint8_t data_num);

protected:

    //遥控器
    virtual int32_t init_usart2(uint32_t baud_rate);

    //光流
    virtual int32_t init_usart4(uint32_t baud_rate);

    //超声波
    virtual int32_t init_usart5(uint32_t baud_rate);


};

#endif




