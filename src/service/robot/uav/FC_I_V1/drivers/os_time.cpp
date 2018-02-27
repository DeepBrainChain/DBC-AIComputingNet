#include "os_time.h"
#include "server.h"
#include "misc.h"
#include "stm32f4xx_rcc.h"


volatile int OSTime::m_time_day = 0;
volatile int OSTime::m_time_hour = 0;
volatile int OSTime::m_time_min = 0;
volatile int OSTime::m_time_second = 0;
volatile int OSTime::m_time_ms = 0;

volatile uint64_t OSTime::SYS_TICK_UP_TME = 0;

extern Server *g_server;


int32_t OSTime::init()
{
    RCC_ClocksTypeDef rcc_clocks;
    RCC_GetClocksFreq(&rcc_clocks);

    uint32_t count = (uint32_t)rcc_clocks.HCLK_Frequency / TICK_PER_SECOND;
    count = count / 8;                   //CLKSOURCE, =0, AHB/8

    //设置sys tick频率
    SysTick_Config(count);

    //CLK Source HCLK AHB/8
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);           //HCLK时钟来源 ,  Div8分频
    
    return E_SUCCESS;
}

uint32_t OSTime::get_os_time_us()
{    
    register uint32_t ms = SYS_TICK_UP_TME;
    uint32_t value = ms * US_PER_TICK + (SysTick->LOAD - SysTick->VAL) * US_PER_TICK / SysTick->LOAD;
    
    return value;
}

void OSTime::delay_us(uint32_t us)
{
    uint32_t now = get_os_time_us();
    
    while (get_os_time_us() - now < us);
}

void OSTime::delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000);
}

void OSTime::update_os_time()
{
    if(!g_server->is_init_ok())
    {
        return;
    }

    //ms
    if(m_time_ms < 999)
    {
        m_time_ms++;
    }
    else
    {          
        m_time_ms = 0;

        //second
        if (m_time_second < 59)
        {
            m_time_second++;
        }
        else
        {
            m_time_second = 0;

            //minute
            if(m_time_min < 59)
            {
                m_time_min++;
            }
            else
            {
                m_time_min = 0;

                //hour
                if(m_time_hour < 23)
                {
                    m_time_hour++;
                }
                else
                {
                    m_time_hour = 0;
                
                    //day
                    m_time_day++;                    
                }
            }
        }
    }
}

uint64_t OSTime::get_sys_tick_up_tme()
{
    return SYS_TICK_UP_TME;
}



