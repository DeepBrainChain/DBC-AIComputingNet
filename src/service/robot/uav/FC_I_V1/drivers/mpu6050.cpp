#include "mpu6050.h"
#include "stm32f4xx_gpio.h"
#include "server.h"
#include "os_time.h"
#include "conf_def.h"
#include "service_def.h"
#include "service_version.h"
#include "FC_I_V1_device_manager.h"



extern Server *g_server;
//sensor_setup_t sensor_setup;

//IMU扩展预留GPIOD PIN7
int32_t MPU6050::init_gpio()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_SetBits(GPIOD, GPIO_Pin_7);

    return E_SUCCESS;
}

uint16_t MPU6050::get_filter(uint16_t lpf)
{
    uint16_t default_filter;
    
    switch(lpf)
    {
    case 5:
        default_filter = MPU6050_DLPF_BW_5;
        break;
    case 10:
        default_filter = MPU6050_DLPF_BW_10;
        break;
    case 20:
        default_filter = MPU6050_DLPF_BW_20;
        break;
    case 42:
        default_filter = MPU6050_DLPF_BW_42;
        break;
    case 98:
        default_filter = MPU6050_DLPF_BW_98;
        break;
    case 188:
        default_filter = MPU6050_DLPF_BW_188;
        break;
    case 256:
        default_filter = MPU6050_DLPF_BW_256;
        break;
    default:
        default_filter = MPU6050_DLPF_BW_42;
        break;
    }

    return default_filter;
}

void MPU6050::set_sleep_enabled(uint8_t enabled)
{
    DEVICE_MANAGER->get_i2c()->write_1bit(MPU6050_ADDR, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, enabled);
}

void MPU6050::set_clock_source(uint8_t source)
{
      DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, source);
}

void MPU6050::set_smplrt_div(uint16_t hz)
{
    DEVICE_MANAGER->get_i2c()->write_1byte(MPU6050_ADDR, MPU6050_RA_SMPLRT_DIV, (1000/hz - 1));
}

void MPU6050::set_full_scale_gyro_range(uint8_t range)
{
    DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, range);
    DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_GYRO_CONFIG, 7, 3, 0x00);        //不自检
}

void MPU6050::set_full_scale_accel_range(uint8_t range)
{
    DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, range);
    DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_ACCEL_CONFIG,7, 3, 0x00);        //不自检
}

void MPU6050::set_dlpf(uint8_t mode)
{
    DEVICE_MANAGER->get_i2c()->write_bits(MPU6050_ADDR, MPU6050_RA_CONFIG, MPU6050_CFG_DLPF_CFG_BIT, MPU6050_CFG_DLPF_CFG_LENGTH, mode);
}

void MPU6050::set_i2c_master_mode_enabled(uint8_t enabled)
{
    DEVICE_MANAGER->get_i2c()->write_1bit(MPU6050_ADDR, MPU6050_RA_USER_CTRL, MPU6050_USERCTRL_I2C_MST_EN_BIT, enabled);
}

void MPU6050::set_i2c_bypass_enabled(uint8_t enabled)
{
    DEVICE_MANAGER->get_i2c()->write_1bit(MPU6050_ADDR, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_I2C_BYPASS_EN_BIT, enabled);
}

int32_t MPU6050::init(uint16_t lpf)
{
    //imu扩展
    init_gpio();

    set_sleep_enabled(0);               //sleep unable
    DELAY_MS(10);

    set_clock_source(MPU6050_CLOCK_PLL_ZGYRO);          //PLL with Z axis gyroscope reference
    DELAY_MS(10);

    set_smplrt_div(1000);               //1000hz
    DELAY_MS(10);
    
    set_full_scale_gyro_range(MPU6050_GYRO_FS_2000);        //+-2000 degree/s
    DELAY_MS(10);

    set_full_scale_accel_range(MPU6050_ACCEL_FS_8);             //+-8G     
    DELAY_MS(10);

    set_dlpf(get_filter(lpf));          //0x04, 20hz, 多旋翼使用
    DELAY_MS(10);

    set_i2c_master_mode_enabled(0);             //由主I2C总线驱动
    DELAY_MS(10);

    //by pass模式
    set_i2c_bypass_enabled(1);          //MPU6050允许MCU访问辅助  I2C 总线
    DELAY_MS(10);

    m_is_ok = true;

    return E_SUCCESS;
}

bool MPU6050::is_ok()
{
    return m_is_ok;
}

int32_t MPU6050::read()
{
    DEVICE_MANAGER->get_i2c()->set_fast_mode(true);

    //ACCEL_X|Y|ZOUT_H|L  + TEMP_OUT_H|L + GYRO_X|Y|ZOUT_H|L
    return DEVICE_MANAGER->get_i2c()->read_nbytes(MPU6050_ADDR, MPU6050_RA_ACCEL_XOUT_H, 14, m_mpu6050_buffer);     
}

void MPU6050::transform(float itx, float ity, float itz, float *it_x, float *it_y, float *it_z)
{
    *it_x = itx;
    *it_y = ity;
    *it_z = itz;
}

int32_t MPU6050::calibrate()
{
#ifdef ACC_ADJ_EN       //是否校准加速度计

    if (m_mpu6050.Acc_CALIBRATE)
    {
        if (my_sqrt(my_pow(m_mpu6050.Acc_I16.x) + my_pow(m_mpu6050.Acc_I16.y) + my_pow(m_mpu6050.Acc_I16.z)) < 2500)
        {
            sensor_setup.Offset.mpu_flag = 1;
        }
        else if (my_sqrt(my_pow(m_mpu6050.Acc_I16.x) + my_pow(m_mpu6050.Acc_I16.y) + my_pow(m_mpu6050.Acc_I16.z)) > 2600)
        {
            sensor_setup.Offset.mpu_flag = 0;
        }

        acc_sum_cnt++;
        sum_temp[A_X] += m_mpu6050.Acc_I16.x;
        sum_temp[A_Y] += m_mpu6050.Acc_I16.y;
        sum_temp[A_Z] += m_mpu6050.Acc_I16.z - 65536/16;   // +-8G的 4096 LSB/g, 减去重力加速度
        sum_temp[TEM] += m_mpu6050.Tempreature;

        if ( acc_sum_cnt >= OFFSET_AV_NUM )
        {
            m_mpu6050.Acc_Offset.x = sum_temp[A_X]/OFFSET_AV_NUM;
            m_mpu6050.Acc_Offset.y = sum_temp[A_Y]/OFFSET_AV_NUM;
            m_mpu6050.Acc_Offset.z = sum_temp[A_Z]/OFFSET_AV_NUM;
            m_mpu6050.Acc_Temprea_Offset = sum_temp[TEM]/OFFSET_AV_NUM;
            
            acc_sum_cnt =0;            
            m_mpu6050.Acc_CALIBRATE = 0;

            //!!!需要补充            
            //f.msg_id = 1;
            //f.msg_data = 1;
            //Param_SaveAccelOffset(&mpu6050.Acc_Offset);
            GET_CONF->save_accel_offset(&m_mpu6050.Acc_Offset);
            
            sum_temp[A_X] = sum_temp[A_Y] = sum_temp[A_Z] = sum_temp[TEM] = 0;
        }
    }

#endif

    if(m_mpu6050.Gyro_CALIBRATE)
    {
        gyro_sum_cnt++;
        sum_temp[G_X] += m_mpu6050.Gyro_I16.x;
        sum_temp[G_Y] += m_mpu6050.Gyro_I16.y;
        sum_temp[G_Z] += m_mpu6050.Gyro_I16.z;
        sum_temp[TEM] += m_mpu6050.Tempreature;

        if ( gyro_sum_cnt >= OFFSET_AV_NUM)
        {
            m_mpu6050.Gyro_Offset.x = (float)sum_temp[G_X]/OFFSET_AV_NUM;
            m_mpu6050.Gyro_Offset.y = (float)sum_temp[G_Y]/OFFSET_AV_NUM;
            m_mpu6050.Gyro_Offset.z = (float)sum_temp[G_Z]/OFFSET_AV_NUM;
            m_mpu6050.Gyro_Temprea_Offset = sum_temp[TEM]/OFFSET_AV_NUM;
            
            gyro_sum_cnt =0;
            
            if(m_mpu6050.Gyro_CALIBRATE)
            {
                //!!!需要补充
                //Param_SaveGyroOffset(&mpu6050.Gyro_Offset);
                //f.msg_id = 2;
                //f.msg_data = 1;
                GET_CONF->save_gyro_offset(&m_mpu6050.Gyro_Offset);
            }
            
            m_mpu6050.Gyro_CALIBRATE = 0;            
            sum_temp[G_X] = sum_temp[G_Y] = sum_temp[G_Z] = sum_temp[TEM] = 0;
        }
    }    

    return E_SUCCESS;
}

int32_t MPU6050::prepare_data(float t)
{
    uint8_t i;
    float Gyro_tmp[3];
    s32 FILT_TMP[ITEMS] = {0,0,0,0,0,0,0};

    calibrate();

    //读取buffer原始数据
    
    //加速度
    m_mpu6050.Acc_I16.x = ((((int16_t)m_mpu6050_buffer[0]) << 8) | m_mpu6050_buffer[1]) ;
    m_mpu6050.Acc_I16.y = ((((int16_t)m_mpu6050_buffer[2]) << 8) | m_mpu6050_buffer[3]) ;
    m_mpu6050.Acc_I16.z = ((((int16_t)m_mpu6050_buffer[4]) << 8) | m_mpu6050_buffer[5]) ;

    //陀螺仪
    m_mpu6050.Gyro_I16.x = ((((int16_t)m_mpu6050_buffer[ 8]) << 8) | m_mpu6050_buffer[ 9]) ;
    m_mpu6050.Gyro_I16.y = ((((int16_t)m_mpu6050_buffer[10]) << 8) | m_mpu6050_buffer[11]) ;
    m_mpu6050.Gyro_I16.z = ((((int16_t)m_mpu6050_buffer[12]) << 8) | m_mpu6050_buffer[13]) ;

    Gyro_tmp[0] = m_mpu6050.Gyro_I16.x ;
    Gyro_tmp[1] = m_mpu6050.Gyro_I16.y ;
    Gyro_tmp[2] = m_mpu6050.Gyro_I16.z ;

    //tempreature
    m_mpu6050.Tempreature = ((((int16_t)m_mpu6050_buffer[6]) << 8) | m_mpu6050_buffer[7]);
    m_mpu6050.TEM_LPF += 2 *3.14f * t *(m_mpu6050.Tempreature - m_mpu6050.TEM_LPF);

    //temprature in degree C
    m_mpu6050.Ftempreature = m_mpu6050.TEM_LPF/340.0f + 36.5f;                     //转化为摄氏度

    //======================================================================
    if ( ++filter_cnt > FILTER_NUM )
    {
        filter_cnt = 0;
        filter_cnt_old = 1;
    }
    else
    {
        filter_cnt_old = (filter_cnt == FILTER_NUM) ?  0 : (filter_cnt + 1);
    }
    
    //10 170 4056
    // 得出校准后的数据
    //accel
    if(sensor_setup.Offset.mpu_flag == 0)           //>2600
    {
        mpu6050_tmp[A_X] = (m_mpu6050.Acc_I16.x - m_mpu6050.Acc_Offset.x) ;                                      //去除初始测量偏移值
        mpu6050_tmp[A_Y] = (m_mpu6050.Acc_I16.y - m_mpu6050.Acc_Offset.y) ;
        mpu6050_tmp[A_Z] = (m_mpu6050.Acc_I16.z - m_mpu6050.Acc_Offset.z) ;
    }
    else
    {
        mpu6050_tmp[A_X] = 2*(m_mpu6050.Acc_I16.x - m_mpu6050.Acc_Offset.x) ;
        mpu6050_tmp[A_Y] = 2*(m_mpu6050.Acc_I16.y - m_mpu6050.Acc_Offset.y) ;
        mpu6050_tmp[A_Z] = 2*(m_mpu6050.Acc_I16.z - m_mpu6050.Acc_Offset.z - 2048) ;
    }

    //gyro
    mpu6050_tmp[G_X] = Gyro_tmp[0] - m_mpu6050.Gyro_Offset.x ;
    mpu6050_tmp[G_Y] = Gyro_tmp[1] - m_mpu6050.Gyro_Offset.y ;
    mpu6050_tmp[G_Z] = Gyro_tmp[2] - m_mpu6050.Gyro_Offset.z ;    

    // 更新滤波滑动窗口数组
    FILT_BUF[A_X][filter_cnt] = mpu6050_tmp[A_X];
    FILT_BUF[A_Y][filter_cnt] = mpu6050_tmp[A_Y];
    FILT_BUF[A_Z][filter_cnt] = mpu6050_tmp[A_Z];
    
    FILT_BUF[G_X][filter_cnt] = mpu6050_tmp[G_X];
    FILT_BUF[G_Y][filter_cnt] = mpu6050_tmp[G_Y];
    FILT_BUF[G_Z][filter_cnt] = mpu6050_tmp[G_Z];

    //均值平均
    for (i=0; i<FILTER_NUM; i++)
    {
        FILT_TMP[A_X] += FILT_BUF[A_X][i];
        FILT_TMP[A_Y] += FILT_BUF[A_Y][i];
        FILT_TMP[A_Z] += FILT_BUF[A_Z][i];
        
        FILT_TMP[G_X] += FILT_BUF[G_X][i];
        FILT_TMP[G_Y] += FILT_BUF[G_Y][i];
        FILT_TMP[G_Z] += FILT_BUF[G_Z][i];
    }

    //accel
    mpu_fil_tmp[A_X] = (float)( FILT_TMP[A_X] )/(float)FILTER_NUM;
    mpu_fil_tmp[A_Y] = (float)( FILT_TMP[A_Y] )/(float)FILTER_NUM;
    mpu_fil_tmp[A_Z] = (float)( FILT_TMP[A_Z] )/(float)FILTER_NUM;

    //gyro
    mpu_fil_tmp[G_X] = (float)( FILT_TMP[G_X] )/(float)FILTER_NUM;
    mpu_fil_tmp[G_Y] = (float)( FILT_TMP[G_Y] )/(float)FILTER_NUM;
    mpu_fil_tmp[G_Z] = (float)( FILT_TMP[G_Z] )/(float)FILTER_NUM;


    //坐标转换
    transform(mpu_fil_tmp[A_X], mpu_fil_tmp[A_Y], mpu_fil_tmp[A_Z], &m_mpu6050.Acc.x, &m_mpu6050.Acc.y, &m_mpu6050.Acc.z);
    transform(mpu_fil_tmp[G_X], mpu_fil_tmp[G_Y], mpu_fil_tmp[G_Z], &m_mpu6050.Gyro.x, &m_mpu6050.Gyro.y, &m_mpu6050.Gyro.z);

    //gyro转化为角度
    m_mpu6050.Gyro_deg.x = m_mpu6050.Gyro.x * TO_ANGLE;             //16.4 LSB / degree/s, 转化为角度 * 1/16.4
    m_mpu6050.Gyro_deg.y = m_mpu6050.Gyro.y * TO_ANGLE;
    m_mpu6050.Gyro_deg.z = m_mpu6050.Gyro.z * TO_ANGLE;

    return E_SUCCESS;
}


