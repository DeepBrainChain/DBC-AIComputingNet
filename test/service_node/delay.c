#include "delay.h"
#include "stm32f10x.h"



static volatile uint32_t time_delay;


void time_decrement(void)
{
	if (time_delay != 0x00)
	{ 
		time_delay--;
	}
}

//void SysTick_Handler(void) 
//{
//	time_decrement();
//}

void sys_tick_init(uint32_t hz_per_second)
{
    if (SysTick_Config(SystemCoreClock / hz_per_second))
    {
        while (true);
    }
}

void delay_us(volatile uint32_t time)
{
    time_delay = time;
    
    SysTick->CTRL |=  SysTick_CTRL_ENABLE_Msk;

    while (time_delay);

    SysTick->CTRL &= ~ SysTick_CTRL_ENABLE_Msk;
}



