#include "os_i2c.h"
#include "common.h"
#include "stm32f4xx_gpio.h"


int32_t I2C::init()
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_AHB1PeriphClockCmd(OS_RCC_I2C, ENABLE);         //clock
    
    GPIO_InitStructure.GPIO_Pin = I2C_Pin_SCL | I2C_Pin_SDA;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    
    GPIO_Init(OS_GPIO_I2C, &GPIO_InitStructure);

    return E_SUCCESS;
}

//SCL高电平, SDA高->低
int32_t I2C::start()
{
    SDA_H;
    SCL_H;
    delay();
    
    if (!SDA_read) return E_DEFAULT;        //SDA线为低电平则总线忙,退出
    
    SDA_L;    
    delay();
    
    if (SDA_read) return E_DEFAULT;         //SDA线为高电平则总线出错,退出
    
    SDA_L;    
    delay();
    
    return E_SUCCESS;
}

//SCL高电平, SDA低->高
int32_t I2C::stop()
{
    SCL_L;
    delay();
    
    SDA_L;
    delay();
    
    SCL_H;
    delay();
    
    SDA_H;
    delay();

    return E_SUCCESS;
}

//SCL低->高->低，SDA低电平
void I2C::ack()
{
    SCL_L;
    delay();
    
    SDA_L;
    delay();
    
    SCL_H;
    delay();
    
    SCL_L;
    delay();
}

//SCL低->高->低，SDA高电平
void I2C::nack()
{
    SCL_L;
    delay();
    
    SDA_H;
    delay();
    
    SCL_H;
    delay();
    
    SCL_L;
    delay();
}

void I2C::delay()
{
    __nop();__nop();__nop();
    __nop();__nop();__nop();
    __nop();__nop();__nop();

    if (!m_fast_mode)
    {
        uint8_t i = DELAY_WAIT_TICK;
        while(i--);
    }
}


//SCL低->高->低，等待SDA低电平
int32_t I2C::wait_ack()
{
    uint8_t ErrTimes = 0;
    
    SCL_L;
    delay();

    SDA_H;
    delay();

    SCL_H;
    delay();
    
    while(SDA_read)
    {
        ErrTimes++;
        
        if(ErrTimes > MAX_WAIT_ERR_TIMES)
        {
            stop();
            return E_DEFAULT;
        }
    }

    SCL_L;
    delay();

    return E_SUCCESS;
}

uint8_t I2C::read_byte(uint8_t is_ack)
{
    uint8_t i = 8;
    uint8_t receive_byte = 0;

    SDA_H;
    
    while(i--)
    {
        receive_byte <<= 1;
        
        SCL_L;
        delay();
        
        SCL_H;
        delay();
        
        if(SDA_read)
        {
            receive_byte |= 0x01;
        }
    }
    
    SCL_L;

    if (is_ack)
    {
        ack();
    }
    else
    {
        nack();
    }
    
    return receive_byte;
}

int32_t I2C::send_byte(uint8_t SendByte)
{
    uint8_t i = 8;
    
    while(i--)
    {
        //SCL 低->高, SDA 根据bit传输高低电平
        SCL_L;
        delay();

        //按照bit传输
        if (SendByte & 0x80)
            SDA_H;
        else 
            SDA_L;
        
        SendByte <<= 1;
        delay();
        
        SCL_H;
        delay();
    }
    
    SCL_L;

    return E_SUCCESS;
}

int32_t I2C::write_1byte(uint8_t slave_address, uint8_t reg_address, uint8_t reg_data)
{
    start();

    //slave address
    send_byte(slave_address << 1);

    //wait ack
    if(wait_ack() < 0)
    {
        stop();
        return E_DEFAULT;
    }

    //send byte
    send_byte(reg_address);
    wait_ack();

    //send byte
    send_byte(reg_data);
    wait_ack();

    stop(); 

    return E_SUCCESS;
}

int32_t I2C::read_1byte(uint8_t slave_address, uint8_t reg_address, uint8_t *reg_data)
{
    start();

    //slave address
    send_byte(slave_address << 1);
    
    if(wait_ack() < 0)
    {
        stop();
        return E_DEFAULT;
    }

    //reg address
    send_byte(reg_address);
    wait_ack();
    
    start();
    
    send_byte(slave_address << 1 | 0x01);
    wait_ack();

    //read byte
    *reg_data= read_byte(I2C_NACK);
    
    stop();
    
    return E_SUCCESS;
}

//此处需要调试!!!
int32_t I2C::write_1bit(uint8_t dev, uint8_t reg, uint8_t bit_num, uint8_t data)
{
    uint8_t b;

    //read byte data
    read_nbytes(dev, reg, 1, &b);

    //set bit value
    b = (0 != data) ? (b | (1 << bit_num)) : (b & ~ (1 << bit_num));

    //write byte data
    return write_1byte(dev, reg, b);
}

//此处需要调试，感觉存在问题!!!
int32_t I2C::write_bits(uint8_t dev, uint8_t reg, uint8_t bit_start, uint8_t length, uint8_t data)
{
    uint8_t b, mask;

    //read byte data
    read_nbytes(dev, reg, 1, &b);

    mask = (0xFF << (bit_start + 1)) | 0xFF >> ((8 - bit_start) + length - 1);
    
    data <<= (8 - length);
    data >>= (7 - bit_start);
    
    b &= mask;
    b |= data;
    
    write_1byte(dev, reg, b);

    return E_SUCCESS;
}


int32_t I2C::write_nbytes(uint8_t slave_address, uint8_t reg_address, uint8_t len, uint8_t *buf)
{
    start();

    //send slave address
    send_byte(slave_address<<1);
    
    if(wait_ack() < 0)
    {
    stop();
    return E_DEFAULT;
    }

    //send reg address
    send_byte(reg_address);
    wait_ack();

    //write bytes
    while(len--) 
    {
        send_byte(*buf++);
        wait_ack();
    }
    
    stop();
    
    return E_SUCCESS;
}

int32_t I2C::read_nbytes(uint8_t slave_address, uint8_t reg_address, uint8_t len, uint8_t *buf)
{
    start();

    //send slave address
    send_byte(slave_address << 1);
    
    if(wait_ack() < 0)
    {
        stop();
        return E_DEFAULT;
    }

    //send reg address
    send_byte(reg_address); 
    wait_ack();

    start();
    
    send_byte(slave_address << 1 | 0x01);
    wait_ack();
    
    while(len)
    {
        if(len == 1)
         {
            *buf = read_byte(I2C_NACK);     //send nack
        }
        else
        {
             *buf = read_byte(I2C_ACK);      //send ack message
        }
        
        buf++;
        len--;
    }
    
    stop();
    
    return E_SUCCESS;
}



