#include "ultrasonic.h"
#include "usart.h"
#include "server.h"
#include "service_version.h"


int32_t Ultrasonic::init()
{
    //串口5初始化，函数参数为波特率
    //return USART_DEVICE->init_usart5(9600);
    return E_SUCCESS;
}

//50ms一次
int32_t Ultrasonic::read()
{
	u8 temp[3];
	m_ultra.h_dt = 0.05f;                                                   

	temp[0] = 0x55;
	USART_DEVICE->send_usart5(temp, 1);             //发送0x55, 启动发送超声波脉冲

	m_ultra_start_f = 1;

	if (m_ultra.measure_ot_cnt < 200)                        //200ms
	{
		m_ultra.measure_ot_cnt += m_ultra.h_dt *1000;
	}
	else
	{
		m_ultra.measure_ok = 0;                                 //超时，复位
	}

	return E_SUCCESS;
}

int32_t Ultrasonic::get(uint8_t com_data)
{
    static uint8_t ultra_tmp = 0;

    //第一个字节
    if (m_ultra_start_f == 1)
    {
        ultra_tmp = com_data;
        m_ultra_start_f = 2;
    }
    //第二个字节
    else if (m_ultra_start_f == 2)
    {
        m_ultra.height =  ((ultra_tmp<<8) + com_data) / 10;

        if(m_ultra.height < 500)                // 5米范围内认为有效，跳变值约10米.
        {
            m_ultra.relative_height = m_ultra.height;
            m_ultra.measure_ok = ULTRASONIC_MEASURE_OK;
        }
        else
        {
            m_ultra.measure_ok = ULTRASONIC_MEASURE_BEYOND_RANGE;               //数据超范围
        }

        m_ultra_start_f = 0;
    }

    m_ultra.measure_ot_cnt = 0;     //清除超时计数（喂狗）
    
    m_ultra.h_delta = m_ultra.relative_height - m_ultra_distance_old;   //记录差值
    
    m_ultra_distance_old = m_ultra.relative_height;     //保存历史数据
    
    return E_SUCCESS;
}



