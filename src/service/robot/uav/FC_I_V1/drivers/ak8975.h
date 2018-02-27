/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : ak8975.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月10日
  最近修改   :
  功能描述   : 本文件描述AK8975磁罗盘驱动
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月10日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _AK8975_H_
#define _AK8975_H_

#include "common.h"
#include "param_def.h"

#define AK8975_ADDRESS              0x0c    // 0x18

#define AK8975_WIA                  0x00
#define AK8975_HXL                  0x03
#define AK8975_HXH                  0x04
#define AK8975_HYL                  0x05
#define AK8975_HYH                  0x06
#define AK8975_HZL                  0x07
#define AK8975_HZH                  0x08
#define AK8975_CNTL                 0x0A

#define CALIBRATING_MAG_CYCLES      2000    //校准时间持续20s

typedef struct 
{
    xyz_i16_t Mag_Adc;                        //采样值
    xyz_f_t Mag_Offset;                      //偏移值
    xyz_f_t Mag_Gain;                          //比例缩放
    xyz_f_t Mag_Val;                          //纠正后的值    
}ak8975_t;



class AK8975
{
public:
    
    AK8975();

    virtual bool is_ok();

    virtual int32_t init();

    virtual void read_data();

    virtual int32_t calibrate();

    virtual void set_need_calibrated() {m_need_calibrated = true;}

protected:

    virtual xyz_f_t get_data();

    virtual int32_t prepare_read();

protected:

    bool m_is_ok;

    bool m_need_calibrated;

    ak8975_t m_ak8975;

};


#endif


