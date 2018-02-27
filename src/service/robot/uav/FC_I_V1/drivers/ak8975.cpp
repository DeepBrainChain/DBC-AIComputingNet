#include "ak8975.h"
#include "os_math.h"
#include "server.h"
#include "service_version.h"

extern Server *g_server;


AK8975::AK8975() : m_is_ok(false), m_need_calibrated(false)
{
    //采样值
    m_ak8975.Mag_Adc.x = 0;
    m_ak8975.Mag_Adc.y = 0;
    m_ak8975.Mag_Adc.z = 0;
    
    //偏移值
    m_ak8975.Mag_Offset.x = -1;
    m_ak8975.Mag_Offset.y = -1;
    m_ak8975.Mag_Offset.z = -1;
    
    //比例缩放
    m_ak8975.Mag_Gain.x = 1;
    m_ak8975.Mag_Gain.y = 0.8538;
    m_ak8975.Mag_Gain.z = 0.9389;
    
    //纠正后的值
    m_ak8975.Mag_Val.x = 0;
    m_ak8975.Mag_Val.y = 0;
    m_ak8975.Mag_Val.z = 0;        
}

int32_t AK8975::init()
{
    return prepare_read();
}

int32_t AK8975::prepare_read()
{
    int32_t result = I2C_DEVICE->write_1byte(AK8975_ADDRESS, AK8975_CNTL, 0x01);
    if (E_SUCCESS == result)
    {
        m_is_ok = true;
    }

    return result;
}

bool AK8975::is_ok()
{
    return m_is_ok;
}

xyz_f_t AK8975::get_data()
{
    return m_ak8975.Mag_Val;
}

void AK8975::read_data()
{
    int16_t mag_temp[3];
    uint8_t ak8975_buffer[6];                                       //接收数据缓存
    
    I2C_DEVICE->set_fast_mode(false);

    //磁力计X轴
    I2C_DEVICE->read_1byte(AK8975_ADDRESS, AK8975_HXL, &ak8975_buffer[0]);
    I2C_DEVICE->read_1byte(AK8975_ADDRESS, AK8975_HXH, &ak8975_buffer[1]);    
    mag_temp[1] = ((((int16_t)ak8975_buffer[1]) << 8) | ak8975_buffer[0]) ;     

    //磁力计Y轴
    I2C_DEVICE->read_1byte(AK8975_ADDRESS,AK8975_HYL,&ak8975_buffer[2]);
    I2C_DEVICE->read_1byte(AK8975_ADDRESS,AK8975_HYH,&ak8975_buffer[3]);    
    mag_temp[0] = ((((int16_t)ak8975_buffer[3]) << 8) | ak8975_buffer[2]) ;     

    //磁力计Z轴 
    I2C_DEVICE->read_1byte(AK8975_ADDRESS,AK8975_HZL,&ak8975_buffer[4]);
    I2C_DEVICE->read_1byte(AK8975_ADDRESS,AK8975_HZH,&ak8975_buffer[5]);    
    mag_temp[2] = -((((int16_t)ak8975_buffer[5]) << 8) | ak8975_buffer[4]) ;       
    
    m_ak8975.Mag_Adc.x = mag_temp[0];
    m_ak8975.Mag_Adc.y = mag_temp[1];
    m_ak8975.Mag_Adc.z = mag_temp[2];
    
    m_ak8975.Mag_Val.x = (m_ak8975.Mag_Adc.x - m_ak8975.Mag_Offset.x);
    m_ak8975.Mag_Val.y = (m_ak8975.Mag_Adc.y - m_ak8975.Mag_Offset.y);
    m_ak8975.Mag_Val.z = (m_ak8975.Mag_Adc.z - m_ak8975.Mag_Offset.z);
    
    //磁力计中点矫正    
    calibrate();
    
    //AK8975采样触发
    prepare_read();
}

//!!!后面需要完善和优化, 采用椭球拟合校正法
int32_t AK8975::calibrate()
{
    static uint16_t cnt_m = 0;
    static xyz_f_t    MagMAX = { -100 , -100 , -100 }, MagMIN = { 100 , 100 , 100 }, MagSum;
    
    if (m_need_calibrated)
    {
        //将采样值规定在小于400的范围内
        if (ABS(m_ak8975.Mag_Adc.x)<400 && ABS(m_ak8975.Mag_Adc.y)<400 && ABS(m_ak8975.Mag_Adc.z)<400)
        {
            //记录最大值
            MagMAX.x = _MAX(m_ak8975.Mag_Adc.x, MagMAX.x);
            MagMAX.y = _MAX(m_ak8975.Mag_Adc.y, MagMAX.y);
            MagMAX.z = _MAX(m_ak8975.Mag_Adc.z, MagMAX.z);

            //记录最小值
            MagMIN.x = _MIN(m_ak8975.Mag_Adc.x, MagMIN.x);
            MagMIN.y = _MIN(m_ak8975.Mag_Adc.y, MagMIN.y);
            MagMIN.z = _MIN(m_ak8975.Mag_Adc.z, MagMIN.z);
            
            if (cnt_m == CALIBRATING_MAG_CYCLES)
            {
                //利用最大值与最小值求偏移值
                m_ak8975.Mag_Offset.x = (int16_t)((MagMAX.x + MagMIN.x) * 0.5f);
                m_ak8975.Mag_Offset.y = (int16_t)((MagMAX.y + MagMIN.y) * 0.5f);
                m_ak8975.Mag_Offset.z = (int16_t)((MagMAX.z + MagMIN.z) * 0.5f);
    
                MagSum.x = MagMAX.x - MagMIN.x;
                MagSum.y = MagMAX.y - MagMIN.y;
                MagSum.z = MagMAX.z - MagMIN.z;

                //以x为基准进行比例缩放
                m_ak8975.Mag_Gain.y = MagSum.x / MagSum.y;
                m_ak8975.Mag_Gain.z = MagSum.x / MagSum.z;

                //保存数据
                GET_CONF->save_mag_offset(&m_ak8975.Mag_Offset);
                
                cnt_m = 0;
                m_need_calibrated = false;
                
                //f.msg_id = 3;
                //f.msg_data = 1;
            }
        }
        
        cnt_m++;
        
    }
    else
    {
        return E_SUCCESS;
    }    

    return E_SUCCESS;
}



