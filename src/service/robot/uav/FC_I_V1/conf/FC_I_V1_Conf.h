/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : FC_I_V1_Conf.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月16日
  最近修改   :
  功能描述   : 本文件负责FC_I_V1版本飞控配置文件管理
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月16日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/


#ifndef _FC_I_V1_CONF_H_
#define _FC_I_V1_CONF_H_


#include "conf.h"
#include "param_def.h"
#include "ff.h"



#define PID_CONF_FILE                    "pid.conf"
#define SENSOR_CONF_FILE           "sensor.conf"


class FC_I_V1_Conf : public Conf
{
public:

    FC_I_V1_Conf() {}

    virtual int32_t init();

    virtual int32_t exit() {return E_SUCCESS;}

    virtual void *get(uint32_t type);

    virtual int32_t save_mag_offset(xyz_f_t *offset);

    virtual int32_t save_accel_offset(xyz_f_t *offset);

    virtual int32_t save_gyro_offset(xyz_f_t *offset);

protected:

    virtual int32_t read_settings();

    virtual int32_t reset_settings();

    virtual int32_t init_settings();

    virtual int32_t write_settings();

    virtual int32_t init_pid();

    virtual int32_t init_ctrl_param();
    
protected:

    FATFS m_fs;
    
    FIL m_file;
    
    DIR m_dir;

    pid_setup_t m_pid_conf;

    sensor_setup_t m_sensor_conf;

};


#endif


