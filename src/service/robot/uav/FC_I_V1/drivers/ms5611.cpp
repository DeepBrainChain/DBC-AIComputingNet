#include "ms5611.h"
#include "os_time.h"
#include "server.h"
#include "filter.h"
#include "service_version.h"
#include "FC_I_V1_device_manager.h"


extern Server *g_server;


int32_t MS5611::init()
{
    DELAY_MS(10);

    reset();

    DELAY_MS(3);

    read_prom();

    start_t();

    m_is_ok = true;

    return E_SUCCESS;
}

bool MS5611::is_ok()
{
    return m_is_ok;
}

int32_t MS5611::reset()
{
    return DEVICE_MANAGER->get_i2c()->write_1byte(MS5611_ADDR, CMD_RESET, 1);
}

int32_t MS5611::read_prom()
{
    uint8_t i;
    uint8_t check = 0;
    uint8_t rxbuf[2] = { 0, 0 };

    for (i = 0; i < PROM_NB; i++)
    {
        //check += DEVICE_MANAGER->get_i2c()->read_nbytes(MS5611_ADDR, CMD_PROM_RD + i * 2, 2, rxbuf);                                //send PROM READ command
        DEVICE_MANAGER->get_i2c()->read_nbytes(MS5611_ADDR, CMD_PROM_RD + i * 2, 2, rxbuf);                                                       //send PROM READ command
        m_ms5611_prom[i] = rxbuf[0] << 8 | rxbuf[1];
    }

    /*if(check == PROM_NB)
    {
        return E_SUCCESS;
    }
    else
    {
        return E_DEFAULT;
    }*/

    return E_SUCCESS;    
}

int32_t MS5611::start_t()
{
    //0x58: 读取数据温差D2 OSR=4096
    return DEVICE_MANAGER->get_i2c()->write_1byte(MS5611_ADDR, CMD_ADC_CONV + CMD_ADC_D2 + MS5611_OSR, 1);   // D2 (temperature) conversion start!
}

int32_t MS5611::start_p()
{
    return DEVICE_MANAGER->get_i2c()->write_1byte(MS5611_ADDR, CMD_ADC_CONV + CMD_ADC_D1 + MS5611_OSR, 1);
}

int32_t MS5611::get_baro_alt()
{
    return m_baro.height;
}

int32_t MS5611::update()
{
    static int32_t state = 0;

    if (state)
    {
        read_adc_p();
        start_t();
        cal_baro_alt();
        
        state = 0;
    }
    else
    {
        read_adc_t();
        start_p();
        
        state = 1;
    }

    return state;
}

void MS5611::read_adc_t()
{
    DEVICE_MANAGER->get_i2c()->read_nbytes(MS5611_ADDR, CMD_ADC_READ, 3, m_t_rxbuf);
}

void MS5611::read_adc_p()
{
    DEVICE_MANAGER->get_i2c()->read_nbytes(MS5611_ADDR, CMD_ADC_READ, 3, m_p_rxbuf);
}

int32_t MS5611::cal_baro_alt()
{
    static uint8_t baro_start;

    int32_t temperature, off2 = 0, sens2 = 0, delt;
    int32_t pressure;

    float alt_3;

    int32_t dT;
    int64_t off;
    int64_t sens;

    static vs32 sum_tmp_5611 = 0;
    static uint8_t sum_cnt = BARO_CAL_CNT + 10;

    m_ms5611_ut = (m_t_rxbuf[0] << 16) | (m_t_rxbuf[1] << 8) | m_t_rxbuf[2];
    m_ms5611_up = (m_p_rxbuf[0] << 16) | (m_p_rxbuf[1] << 8) | m_p_rxbuf[2];

    dT = m_ms5611_ut - ((uint32_t)m_ms5611_prom[5] << 8);
    off = ((uint32_t)m_ms5611_prom[2] << 16) + (((int64_t)dT * m_ms5611_prom[4]) >> 7);
    sens = ((uint32_t)m_ms5611_prom[1] << 15) + (((int64_t)dT * m_ms5611_prom[3]) >> 8);

    //get temprature
    temperature = 2000 + (((int64_t)dT * m_ms5611_prom[6]) >> 23);

    if (temperature < 2000)  // temperature lower than 20degC 
    {
        delt = temperature - 2000;
        delt = delt * delt;
        off2 = (5 * delt) >> 1;
        sens2 = (5 * delt) >> 2;

        if (temperature < -1500)  // temperature lower than -15degC
        {
            delt = temperature + 1500;
            delt = delt * delt;
            off2  += 7 * delt;
            sens2 += (11 * delt) >> 1;
        }
    }

    off  -= off2; 
    sens -= sens2;

    //calculate pressure
    pressure = (((m_ms5611_up * sens ) >> 21) - off) >> 15;

    //计算气压
    alt_3 = (101000 - pressure)/1000.0f;                    //计算气压参数
    pressure = 0.82f *alt_3 * alt_3 *alt_3 + 0.09f *(101000 - pressure)*100.0f ;            //12000米以下，与真实气压曲线相吻合
    
    m_baro.height = (int32_t)( pressure );              //( my_deathzoom(( pressure - baro_Offset ), baroAlt ,baro_fix ) ) ; //cm + 
    m_baro.relative_height = Moving_Median(m_mo_av_baro, MO_LEN, &m_mo_av_cnt, (int32_t)(pressure - m_baro_Offset));              //计算相对高度

    m_baro.h_delta = ( m_baro.height - m_baro_height_old ) ;
    m_baro_height_old = m_baro.height;
    
    //m_baro.measure_ok = 1;

    if( baro_start < 50 )
    {
        baro_start++;
        
        m_baro.h_delta = 0;
        m_baro.relative_height = 0;
        
        if(baro_start < 10)
        {
            m_baro_Offset = pressure;
        }
        else
        {
            m_baro_Offset += 10.0f *3.14f *0.02f *(pressure - m_baro_Offset);
        }
    }

    if(sum_cnt)
    {
        sum_cnt--;
    }
    else
    {
        m_temperature_5611 += 0.01f *( ( 0.01f *temperature ) - m_temperature_5611 );
    }
    
    return E_SUCCESS;
}



