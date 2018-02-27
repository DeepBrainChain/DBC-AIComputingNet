/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : ms5611.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月4日
  最近修改   :
  功能描述   : 本文件负责MS5611气压计的接口
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月4日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _MS5611_H_
#define _MS5611_H_

#include "common.h"


//address
#define MS5611_ADDR             0x77        //0xEE

//CMD
#define CMD_RESET                   0x1E        // ADC reset command
#define CMD_ADC_READ         0x00         // ADC read command
#define CMD_ADC_CONV        0x40        // ADC conversion command
#define CMD_ADC_D1               0x00        // ADC D1 conversion
#define CMD_ADC_D2               0x10       // ADC D2 conversion
#define CMD_ADC_256             0x00        // ADC OSR=256
#define CMD_ADC_512             0x02        // ADC OSR=512
#define CMD_ADC_1024           0x04        // ADC OSR=1024
#define CMD_ADC_2048           0x06        // ADC OSR=2048
#define CMD_ADC_4096           0x08        // ADC OSR=4096
#define CMD_PROM_RD          0xA0       // Prom read command

#define PROM_NB                     8

#define MS5611_OSR              0x08        //CMD_ADC_4096

#define BARO_CAL_CNT         80
#define MO_LEN                        5



typedef struct
{
    int32_t height;                                         //cm

    float relative_height;
    float h_delta;
    float h_dt;

    //uint8_t measure_ok;
    uint8_t measure_ot_cnt;
    
}_HEIGHT_;


class MS5611
{
public:

    MS5611() : m_is_ok(false) {}

    virtual int32_t init();

    virtual bool is_ok();
    
    virtual int32_t reset();

    virtual int32_t start_t();

    virtual int32_t start_p();

    virtual void read_adc_t();

    virtual void read_adc_p();

    virtual int32_t update();    

    virtual int32_t read_prom();

    virtual int32_t cal_baro_alt();

    virtual int32_t get_baro_alt();

protected:

    bool m_is_ok;

    _HEIGHT_ m_baro;                                //气压计高度信息

    int32_t m_baro_Offset;

    uint32_t m_ms5611_ut;                     // static result of temperature measurement
    
    uint32_t m_ms5611_up;                     // static result of pressure measurement
    
    uint16_t m_ms5611_prom[PROM_NB];       // on-chip ROM

    int32_t m_baro_height_old;

    uint8_t m_t_rxbuf[3], m_p_rxbuf[3];

    int32_t m_mo_av_baro[MO_LEN];

    uint16_t m_mo_av_cnt;

    float m_temperature_5611;

};

#endif


