#include "FC_I_V1_device_manager.h"
#include "os_i2c.h"
#include "pwm.h"
#include "led.h"
#include "usart.h"
#include "os_usb_hid.h"
#include "ms5611.h"
#include "mpu6050.h"
#include "ak8975.h"
#include "ultrasonic.h"

FC_I_V1_DeviceManager::FC_I_V1_DeviceManager()
{    
    create_device();
}

//factory method
void FC_I_V1_DeviceManager::create_device()
{
    m_i2c = new I2C();

    m_usart = new USART();
    
    m_led = new LED();

    m_pwm = new PWM(0, 1000, DEFAULT_MOTORS);

    m_ms5611 = new MS5611();

    m_ak8975 = new AK8975();

    m_mpu6050 = new MPU6050();

    m_usb_hid = new USBHID();

    m_ultrasonic = new Ultrasonic();

}


I2C *FC_I_V1_DeviceManager::get_i2c()
{
    return m_i2c;
}

PWM *FC_I_V1_DeviceManager::get_pwm()
{
    return m_pwm;
}

LED *FC_I_V1_DeviceManager::get_led()
{
    return m_led;
}

USART *FC_I_V1_DeviceManager::get_usart()
{
    return m_usart;
}

MS5611 *FC_I_V1_DeviceManager::get_ms5611()
{
    return m_ms5611;
}

AK8975 *FC_I_V1_DeviceManager::get_ak8975()
{
    return m_ak8975;
}

USBHID *FC_I_V1_DeviceManager::get_usb_hid()
{
    m_usb_hid = new USBHID();
	  return m_usb_hid;
}

MPU6050 *FC_I_V1_DeviceManager::get_mpu6050()
{
    return m_mpu6050;
}

Ultrasonic *FC_I_V1_DeviceManager::get_ultrasonic()
{
    return m_ultrasonic;
}


