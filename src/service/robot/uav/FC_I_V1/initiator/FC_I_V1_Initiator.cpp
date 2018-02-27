#include "FC_I_V1_Initiator.h"
#include "server.h"
#include "os_time.h"
#include "stm32f4xx_gpio.h"
#include "FC_I_V1_alarm.h"
#include "FC_I_V1_device_manager.h"
#include "FC_I_V1_conf.h"
#include "service_version.h"


extern Server *g_server;


int32_t FC_I_V1_Initiator::init()
{
    init_device_manager();
    
    init_nvic();

    init_os_clk();
    
    init_i2c();

    init_pwm();

    init_usb_hid();

    init_ms5611();

    DELAY_MS(400);

    init_mpu6050();

    init_led();

    init_usart();

    init_conf(); 

    DELAY_MS(100);

    init_ak8975();

    init_ultrasonic();

    //init service

    init_alarm();

    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::exit()
{
    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::init_conf() 
{
    Conf *conf = new FC_I_V1_Conf();

    //提供全局访问
    g_server->set_conf(conf);

    //初始化
    return conf->init();
}

int32_t FC_I_V1_Initiator::init_alarm() 
{
    g_server->set_alarm(new FC_I_V1_Alarm());
    return E_SUCCESS;
}


int32_t FC_I_V1_Initiator::init_device_manager() 
{
    g_server->set_device_manager(new FC_I_V1_DeviceManager());
    return E_SUCCESS;
}


int32_t FC_I_V1_Initiator::init_nvic()
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);
    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::init_os_clk()
{
    return OSTime::init();
}

int32_t FC_I_V1_Initiator::init_i2c()
{
    return I2C_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_pwm()
{
    return PWM_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_usb_hid()
{
    return USB_HID_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_ms5611()
{
    MS5611_DEVICE->init();

    if (!MS5611_DEVICE->is_ok())
    {
        GET_ALARM->alarm_critical(ALARM_CRITICAL_BARO_ERR);
    }

    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::init_mpu6050()
{
    MPU6050_DEVICE->init(20);

    if (!MPU6050_DEVICE->is_ok())
    {
        GET_ALARM->alarm_critical(ALARM_CRITICAL_MPU_ERR);
    }

    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::init_ak8975()
{
    AK8975_DEVICE->init();

    if (!AK8975_DEVICE->is_ok())
    {
        GET_ALARM->alarm_critical(ALARM_CRITICAL_MAG_ERR);
    }

    return E_SUCCESS;
}

int32_t FC_I_V1_Initiator::init_led()
{
    return LED_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_usart()
{
    return USART_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_ultrasonic()
{
    return ULTRASONIC_DEVICE->init();
}

int32_t FC_I_V1_Initiator::init_service()
{
    return E_SUCCESS;
}


/*

int32_t FC_I_V1_Initiator::init_rcc()
{

}

int32_t FC_I_V1_Initiator::init_gpio()
{

}

int32_t FC_I_V1_Initiator::init_adc()
{

}


int32_t FC_I_V1_Initiator::init_servo()
{

}


int32_t FC_I_V1_Initiator::init_usart_osd()
{

}

int32_t FC_I_V1_Initiator::init_receiver()
{

}

int32_t FC_I_V1_Initiator::init_rpm()
{

}

int32_t FC_I_V1_Initiator::init_timer()
{

}

int32_t FC_I_V1_Initiator::init_data_link()
{

}

int32_t FC_I_V1_Initiator::init_gps()
{

}

*/


