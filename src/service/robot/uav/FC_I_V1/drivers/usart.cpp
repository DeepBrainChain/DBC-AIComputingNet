#include "usart.h"
#include "stm32f4xx_usart.h"


uint8_t count2 = 0;
uint8_t tx2_counter = 0;
uint8_t tx2_buffer[256];
uint8_t rx2_buffer[256];                                                    //??????

u8 count4 = 0;
u8 tx4_buffer[256];
u8 tx4_counter = 0;

u8 count5 = 0;
u8 tx5_buffer[256];
u8 tx5_counter = 0;


int32_t USART::init()
{
    init_usart2(DEFAULT_BAUD_RATE);
    init_usart4(DEFAULT_BAUD_RATE);
    init_usart5(9600);

    return E_SUCCESS;
}

int32_t USART::init_usart2(uint32_t baud_rate)
{
    USART_InitTypeDef USART_InitStructure;
    USART_ClockInitTypeDef USART_ClockInitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    //开启USART2时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);              
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    //串口中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    //GPIO PIN脚复用
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);

    //配置PD5作为USART2Tx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;                  //推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    //配置PD6作为USART2　Rx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;              //是否修改为GPIO_Mode_IN_FLOATING, 浮空输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    //配置USART2
    USART_InitStructure.USART_BaudRate = baud_rate;                                                 //波特率可以通过地面站配置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                 //8位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                                  //在帧结尾传输1个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;                                         //禁用奇偶校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流控制失能
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;     //发送、接收使能

    //配置USART2时钟
    USART_ClockInitStruct.USART_Clock = USART_Clock_Disable;                        //时钟低电平活动
    USART_ClockInitStruct.USART_CPOL = USART_CPOL_Low;                              //SLCK引脚上时钟输出的极性->低电平
    USART_ClockInitStruct.USART_CPHA = USART_CPHA_2Edge;                        //时钟第二个边沿进行数据捕获
    USART_ClockInitStruct.USART_LastBit = USART_LastBit_Disable;                //最后一位数据的时钟脉冲不从SCLK输出

    USART_Init(USART2, &USART_InitStructure);
    USART_ClockInit(USART2, &USART_ClockInitStruct);

    //使能USART2接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    //使能USART2
    USART_Cmd(USART2, ENABLE);    
    
    return E_SUCCESS;
}

int32_t USART::init_usart4(uint32_t baud_rate)
{
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    //开启USART4时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);               
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    //串口中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    //!!!GPIO_AF_UART4
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_UART5);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_UART5);

    //配置PC12作为UART4　Tx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //配置PD2作为UART4　Rx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //配置UART5
    USART_InitStructure.USART_BaudRate = baud_rate;                     //波特率可以通过地面站配置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;         //8位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;              //在帧结尾传输1个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;                 //禁用奇偶校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流控制失能
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;     //发送、接收使能
    USART_Init(UART4, &USART_InitStructure);

    //使能UART5接收中断
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);

    //使能USART5
    USART_Cmd(UART4, ENABLE);   
    
    return E_SUCCESS;
}

int32_t USART::init_usart5(uint32_t baud_rate)
{
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);               //开启USART5时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    //串口中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_UART5);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_UART5);

    //配置PC12作为UART5　Tx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    //配置PD2作为UART5　Rx
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    //配置UART5
    USART_InitStructure.USART_BaudRate = baud_rate;                     //波特率可以通过地面站配置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;         //8位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;              //在帧结尾传输1个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;                 //禁用奇偶校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //硬件流控制失能
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;     //发送、接收使能
    USART_Init(UART5, &USART_InitStructure);

    //使能UART5接收中断
    USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);

    //使能USART5
    USART_Cmd(UART5, ENABLE);    
    
    return E_SUCCESS;
}

int32_t USART::send_usart2(unsigned char *DataToSend, uint8_t data_num)
{
    uint8_t i;
    
    for(i = 0; i < data_num; i++)
    {
        tx2_buffer[count2++] = *(DataToSend + i);
    }

    if (!(USART2->CR1 & USART_CR1_TXEIE))
    {
        USART_ITConfig(USART2, USART_IT_TXE, ENABLE);                   //打开发送中断
    }
}

int32_t USART::send_usart4(unsigned char *DataToSend, uint8_t data_num)
{
    return E_SUCCESS;    
}

int32_t USART::send_usart5(unsigned char *DataToSend, uint8_t data_num)
{
    uint8_t i;
    
    for(i = 0; i < data_num; i++)
    {
        tx5_buffer[count5++] = *(DataToSend+i);
    }

    if (!(UART5->CR1 & USART_CR1_TXEIE))
    {
        USART_ITConfig(UART5, USART_IT_TXE, ENABLE);                    //打开发送中断
    }    
}

void Usart2_IRQ(void)
{
    uint8_t com_data;

    if (USART2->SR & USART_SR_ORE)
    {
        com_data = USART2->DR;
    }

    //接收中断
    if (USART_GetITStatus(USART2, USART_IT_RXNE))
    {
        USART_ClearITPendingBit(USART2,USART_IT_RXNE);                  //清除中断标志
        com_data = USART2->DR;

        //receive prepare!!! 接收数据
        //遥控器
    }

    //发送（进入移位）中断
    if (USART_GetITStatus(USART2, USART_IT_TXE ))
    {
        USART2->DR = tx2_buffer[tx2_counter++];                         //写DR清除中断标志
        
            if(tx2_counter == count2)
            {
                USART2->CR1 &= ~USART_CR1_TXEIE;                            //关闭TXE发送中断
            }
    }
    
}

void Uart4_IRQ(void)
{
    uint8_t com_data;

    //接收中断
    if (USART_GetITStatus(UART4, USART_IT_RXNE))
    {
        USART_ClearITPendingBit(UART4, USART_IT_RXNE);                   //清除中断标志

        com_data = UART4->DR;

        //!!! 光流
        //AnoOF_GetOneByte(com_data);
    }

    //发送（进入移位）中断
    if (USART_GetITStatus(UART4, USART_IT_TXE))
    {
        UART4->DR = tx4_buffer[tx4_counter++];                          //写DR清除中断标志

        if(tx4_counter == count4)
        {
            UART4->CR1 &= ~USART_CR1_TXEIE;                                //关闭TXE（发送中断）中断
        }
    }
    
}

void Uart5_IRQ(void)
{
    uint8_t com_data;

    //接收中断
    if(USART_GetITStatus(UART5, USART_IT_RXNE))
    {
        USART_ClearITPendingBit(UART5, USART_IT_RXNE);                   //清除中断标志

        com_data = UART5->DR;

        //!!!超声波
        //Ultra_Get(com_data);
    }

    //发送（进入移位）中断
    if(USART_GetITStatus(UART5, USART_IT_TXE))
    {
                
        UART5->DR = tx5_buffer[tx5_counter++];                          //写DR清除中断标志
          
        if(tx5_counter == count5)
        {
            UART5->CR1 &= ~USART_CR1_TXEIE;                                //关闭TXE（发送中断）中断
        }
    }    
}



