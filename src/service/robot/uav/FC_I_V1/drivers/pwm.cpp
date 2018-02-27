#include "pwm.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx.h"


volatile uint16_t g_rc_pwm_in[8];


//遥控器接收机输入
//主频168MHz, 2分频为84MHz, 预分频系数84, 分频到 84000000/84 =1MHz   1us
int32_t PWM::init()
{
    init_in();                  //遥控器输入
    
    init_out(400);      //400HZ
    
    return E_SUCCESS;
}

//遥控器接收机输入
int32_t PWM::init_in()
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    //TIM3, TIM4 RCC

    //GPIO Config: GPIO_B, C, D
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3|RCC_APB1Periph_TIM4, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOD, ENABLE);


    //TIM3 NVIC
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    //TIM4 NVIC
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /////////////////////////////////////////////////////////////////////////////////////////////

    //GPIOC Pin6 + Pin7
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;

    GPIO_Init(GPIOC, &GPIO_InitStructure);

    //GPIOB Pin0 + Pin1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;

    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //TIMER Config
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_TIM3);

    TIM3->PSC = (168 / 2) - 1;        //分频系数 84, 时钟计数器    CK_CNT = fCK_PSC / (PSC + 1)

    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);

    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);

    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);

    TIM_Cmd(TIM3, ENABLE);

    TIM_ITConfig(TIM3, TIM_IT_CC1, ENABLE);
    TIM_ITConfig(TIM3, TIM_IT_CC2, ENABLE);
    TIM_ITConfig(TIM3, TIM_IT_CC3, ENABLE);
    TIM_ITConfig(TIM3, TIM_IT_CC4, ENABLE);

    
    /////////////////////////////////////////////////////////////////////////////////////////////
    
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;

    //GPIOD
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_TIM4);
    
    TIM4->PSC = (168 / 2)-1;
        
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
      
    /* TIM enable counter */
    TIM_Cmd(TIM4, ENABLE);
    
    /* Enable the CC2 Interrupt Request */
    TIM_ITConfig(TIM4, TIM_IT_CC1, ENABLE);
    TIM_ITConfig(TIM4, TIM_IT_CC2, ENABLE);
    TIM_ITConfig(TIM4, TIM_IT_CC3, ENABLE);
    TIM_ITConfig(TIM4, TIM_IT_CC4, ENABLE);

    return E_SUCCESS;
}

//电调输出
//主频168MHz, 2分频为84MHz, 预分频系数21, 分频到 84000000/21 = 4MHz   0.25us; 10000产生一次中断, 频率400Hz
//hz: 400Hz
int32_t PWM::init_out(uint16_t hz)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    uint16_t PrescalerValue = 0;
    uint32_t hz_set = ACCURACY * hz;

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_OCStructInit(&TIM_OCInitStructure);

    hz_set = LIMIT(hz_set, 1, 84000000);

    //RCC时钟使能: GPIO_A, C, E
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOE, ENABLE);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /////////////////////////////////////////////////////////////////////////////
    //配置引脚: GPIO Config
    //GPIOA GPIO_Pin_2|GPIO_Pin_3 TIM5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //TIM Pin脚复用: GPIO_A Pin 2, Pin 3
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_TIM5);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_TIM5);

    //使能TIM时钟
    //Compute the prescaler value
    PrescalerValue = (uint16_t) (( SystemCoreClock / 2) / hz_set) - 1;      //CK_CNT = fCK_PSC/(PSC[15:0]+1), 分频系数 21, CK_CNT为4MHz

    //不分频的话，时钟是72mhz，就是每秒计数72m, 
    //而TIM_Period就是定义了pwm的一个周期记的次数，比如说是2000，就是经过2000/72m这个时间是一个周期，
    //那么频率就是周期的倒数，这个pwm的频率就是72m/2000（hz），这样就确定了频率。
    TIM_TimeBaseStructure.TIM_Period = ACCURACY;                //自动重装载的计数器数值: 10000，从0开始计数，达到则产生中断; 400Hz 

    //时钟频率=72MHZ/(时钟预分频+1)。
    //（假设72MHZ为系统运行的频率，这里的时钟频率即是产生这个pwm的时钟的频率）说明当前设置的这个TIM_Prescaler，直接决定定时器的时钟频率。
    //通俗点说，就是一秒钟能计数多少次。比如算出来的时钟频率是2000，也就是
    //一秒钟会计数2000 次，而此时如果TIM_Period 设置为4000，即4000 次计数后就会中断一次。由于时钟频率是一秒钟计数2000 次，因此只要2 秒钟，就会中断一次。
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;       //预分频系数 21，频率为4MHz
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    
    TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

    //设置PWM模式
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    /* PWM1 Mode configuration: Channel3 */
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;       //使能比较输出
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;                          //设置待装入捕获比较寄存器的脉冲值，设置占空比
    TIM_OC3Init(TIM5, &TIM_OCInitStructure);

    //启动预装载
    TIM_OC3PreloadConfig(TIM5, TIM_OCPreload_Enable);

    /* PWM1 Mode configuration: Channel4 */
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC4Init(TIM5, &TIM_OCInitStructure);

    //启动预装载
    TIM_OC4PreloadConfig(TIM5, TIM_OCPreload_Enable);  

    //使能TIMx在ARR上的预装载寄存器
    TIM_ARRPreloadConfig(TIM5, ENABLE);

    //打开定时器
    TIM_Cmd(TIM5, ENABLE);

    /////////////////////////////////////////////////////////////////////////////
    //配置引脚
    //GPIOE GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14 TIM1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    //TIM Pin脚复用
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_TIM1);

    //使能TIM时钟
    /* Compute the prescaler value */
    PrescalerValue = (uint16_t) ( ( SystemCoreClock ) / hz_set ) - 1;
    
    /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = ACCURACY;
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    //设置PWM模式
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    /* PWM1 Mode configuration: Channel1 */
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    //TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);     //TIM1不需要预装载

    /* PWM1 Mode configuration: Channel2 */
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);
    //TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* PWM1 Mode configuration: Channel3 */
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC3Init(TIM1, &TIM_OCInitStructure);
    //TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* PWM1 Mode configuration: Channel4 */
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);
    //TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable);

    //设置TIM的PWM输出为使能(TIM1和TIM8需要设置, 其他TIM不需要)
    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    //使能TIMx在ARR上的预装载寄存器
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    
    TIM_Cmd(TIM1, ENABLE);	

    ////////////////////////////////////////////////////////////////////////////////////
    //GPIOC GPIO_Pin_8 | GPIO_Pin_9 TIM8
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
    GPIO_Init(GPIOC, &GPIO_InitStructure); 

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_TIM8);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_TIM8);

    /* Compute the prescaler value */
    PrescalerValue = (uint16_t) ( ( SystemCoreClock ) / hz_set ) - 1;
    
    /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = ACCURACY;
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);


    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    /* PWM1 Mode configuration: Channel3 */
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC3Init(TIM8, &TIM_OCInitStructure);
    //TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* PWM1 Mode configuration: Channel4 */
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = INIT_PULSE;
    TIM_OC4Init(TIM8, &TIM_OCInitStructure);
    //TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_CtrlPWMOutputs(TIM8, ENABLE);
    TIM_ARRPreloadConfig(TIM8, ENABLE);
    
    TIM_Cmd(TIM8, ENABLE);

    return E_SUCCESS;
}

int32_t PWM::set_pwm(int16_t pwm[MAX_MOTORS])
{
    uint8_t i;
    int16_t pwm_temp[MAX_MOTORS];
    
    for(i = 0; i < m_motor_count; i++)
    {
        pwm_temp[i] = LIMIT(pwm[i], m_min, m_max);     //(0, 1000)
    }

    TIM1->CCR4 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[0]] ) + INIT_PULSE;             // 1  [4000-8000]
    TIM1->CCR3 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[1]] ) + INIT_PULSE;             // 2
    TIM1->CCR2 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[2]] ) + INIT_PULSE;             // 3
    TIM1->CCR1 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[3]] ) + INIT_PULSE;             // 4

    //TIM5->CCR4 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[4]] ) + INIT_PULSE;       // 5
    //TIM5->CCR3 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[5]] ) + INIT_PULSE;       // 6

    //TIM8->CCR4 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[6]] ) + INIT_PULSE;       // 7
    //TIM8->CCR3 = PWM_RADIO * ( pwm_temp[m_ch_out_mapping[7]] ) + INIT_PULSE;       // 8

    return E_SUCCESS;
}

//RC遥控器接收
void _TIM3_IRQHandler(void)
{
    static uint16_t temp_cnt1, temp_cnt1_2, temp_cnt2, temp_cnt2_2, temp_cnt3, temp_cnt3_2, temp_cnt4, temp_cnt4_2;

    //Feed_Rc_Dog(1);                 //RC!!!

    if (TIM3->SR & TIM_IT_CC1)                          //捕获1发生捕获事件
    {
        TIM3->SR = ~TIM_IT_CC1;                         //TIM_ClearITPendingBit(TIM3, TIM_IT_CC1);
        TIM3->SR = ~TIM_FLAG_CC1OF;

        if (GPIOC->IDR & GPIO_Pin_6)              //获取上升沿的计算值
        {
            temp_cnt1 = TIM_GetCapture1(TIM3);
        }
        else
        {
            temp_cnt1_2 = TIM_GetCapture1(TIM3);            //获取下降沿的计算值

            if (temp_cnt1_2  >=  temp_cnt1)
            {
                g_rc_pwm_in[0] = temp_cnt1_2 - temp_cnt1;
            }
            else
            {
                g_rc_pwm_in[0] = 0xffff-temp_cnt1 + temp_cnt1_2 + 1;
            }
        }
    }

    if(TIM3->SR & TIM_IT_CC2) 
    {
        TIM3->SR = ~TIM_IT_CC2;
        TIM3->SR = ~TIM_FLAG_CC2OF;

        if(GPIOC->IDR & GPIO_Pin_7)
        {
            temp_cnt2 = TIM_GetCapture2(TIM3);
        }
        else
        {
            temp_cnt2_2 = TIM_GetCapture2(TIM3);

            if (temp_cnt2_2>=temp_cnt2)
            {
                g_rc_pwm_in[1] = temp_cnt2_2-temp_cnt2;
            }
            else
            {
                g_rc_pwm_in[1] = 0xffff-temp_cnt2+temp_cnt2_2+1;
            }
        }
    }

    if(TIM3->SR & TIM_IT_CC3) 
    {
        TIM3->SR = ~TIM_IT_CC3;
        TIM3->SR = ~TIM_FLAG_CC3OF;

        if (GPIOB->IDR & GPIO_Pin_0)
        {
            temp_cnt3 = TIM_GetCapture3(TIM3);
        }
        else
        {
            temp_cnt3_2 = TIM_GetCapture3(TIM3);

            if (temp_cnt3_2>=temp_cnt3)
            {
                g_rc_pwm_in[2] = temp_cnt3_2-temp_cnt3;
            }
            else
            {
                g_rc_pwm_in[2] = 0xffff-temp_cnt3+temp_cnt3_2+1;
            }
        }
    }

    if(TIM3->SR & TIM_IT_CC4) 
    {
        TIM3->SR = ~TIM_IT_CC4;
        TIM3->SR = ~TIM_FLAG_CC4OF;

        if (GPIOB->IDR & GPIO_Pin_1)
        {
            temp_cnt4 = TIM_GetCapture4(TIM3);
        }
        else
        {
            temp_cnt4_2 = TIM_GetCapture4(TIM3);
            if (temp_cnt4_2>=temp_cnt4)
            {
                g_rc_pwm_in[3] = temp_cnt4_2-temp_cnt4;
            }
            else
            {
                g_rc_pwm_in[3] = 0xffff-temp_cnt4+temp_cnt4_2+1;
            }
        }
    }
}

void _TIM4_IRQHandler(void)
{
    static uint16_t temp_cnt1,temp_cnt1_2,temp_cnt2,temp_cnt2_2,temp_cnt3,temp_cnt3_2,temp_cnt4,temp_cnt4_2;

    //Feed_Rc_Dog(1);                 //RC!!!

    if(TIM4->SR & TIM_IT_CC1) 
    {
        TIM4->SR = ~TIM_IT_CC1;     //TIM_ClearITPendingBit(TIM3, TIM_IT_CC1);
        TIM4->SR = ~TIM_FLAG_CC1OF;

        if (GPIOD->IDR & GPIO_Pin_12)
        {
            temp_cnt1 = TIM_GetCapture1(TIM4);
        }
        else
        {
            temp_cnt1_2 = TIM_GetCapture1(TIM4);

            if (temp_cnt1_2>=temp_cnt1)
            {
                g_rc_pwm_in[4] = temp_cnt1_2-temp_cnt1;
            }
            else
            {
                g_rc_pwm_in[4] = 0xffff-temp_cnt1+temp_cnt1_2+1;
            }
        }
    }

    if(TIM4->SR & TIM_IT_CC2) 
    {
        TIM4->SR = ~TIM_IT_CC2;
        TIM4->SR = ~TIM_FLAG_CC2OF;

        if (GPIOD->IDR & GPIO_Pin_13)
        {
            temp_cnt2 = TIM_GetCapture2(TIM4);
        }
        else
        {
            temp_cnt2_2 = TIM_GetCapture2(TIM4);
            if (temp_cnt2_2>=temp_cnt2)
            {
                g_rc_pwm_in[5] = temp_cnt2_2-temp_cnt2;
            }
            else
            {
                g_rc_pwm_in[5] = 0xffff-temp_cnt2+temp_cnt2_2+1;
            }
        }
    }

    if(TIM4->SR & TIM_IT_CC3) 
    {
        TIM4->SR = ~TIM_IT_CC3;
        TIM4->SR = ~TIM_FLAG_CC3OF;

        if (GPIOD->IDR & GPIO_Pin_14)
        {
            temp_cnt3 = TIM_GetCapture3(TIM4);
        }
        else
        {
            temp_cnt3_2 = TIM_GetCapture3(TIM4);
            if (temp_cnt3_2>=temp_cnt3)
            {
                g_rc_pwm_in[6] = temp_cnt3_2-temp_cnt3;
            }
            else
            {
                g_rc_pwm_in[6] = 0xffff-temp_cnt3+temp_cnt3_2+1;
            }
        }
    }

    if(TIM4->SR & TIM_IT_CC4) 
    {
        TIM4->SR = ~TIM_IT_CC4;
        TIM4->SR = ~TIM_FLAG_CC4OF;

        if (GPIOD->IDR & GPIO_Pin_15)
        {
            temp_cnt4 = TIM_GetCapture4(TIM4);
        }
        else
        {
            temp_cnt4_2 = TIM_GetCapture4(TIM4);
            if (temp_cnt4_2>=temp_cnt4)
            {
                g_rc_pwm_in[7] = temp_cnt4_2-temp_cnt4;
            }
            else
            {
                g_rc_pwm_in[7] = 0xffff-temp_cnt4+temp_cnt4_2+1;
            }
        }
    }
}


