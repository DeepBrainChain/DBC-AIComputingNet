#include "led.h"
#include "stm32f4xx_gpio.h"

int32_t LED::init()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(OS_RCC_LED,ENABLE);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_Pin   = OS_Pin_LED1| OS_Pin_LED2| OS_Pin_LED3| OS_Pin_LED4;
    GPIO_Init(OS_GPIO_LED, &GPIO_InitStructure);

    GPIO_SetBits(OS_GPIO_LED, OS_Pin_LED1);
    GPIO_SetBits(OS_GPIO_LED, OS_Pin_LED2);
    GPIO_SetBits(OS_GPIO_LED, OS_Pin_LED3);
    GPIO_SetBits(OS_GPIO_LED, OS_Pin_LED4); 

    return E_SUCCESS;	
}


