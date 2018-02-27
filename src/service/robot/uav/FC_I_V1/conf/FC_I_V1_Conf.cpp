#include "FC_I_V1_Conf.h"
#include "ff.h"
#include "ctrl_def.h"
#include "pid_def.h"
#include "conf_def.h"
#include "service_version.h"
#include "FC_I_V1_device_manager.h"
#include "server.h"

extern Server *g_server;

int32_t FC_I_V1_Conf::init() 
{
    //read setting
    int32_t result = read_settings();
    if (E_SUCCESS != result)
    {
        reset_settings();               //reset
        return E_DEFAULT;
    }

    //init setting
    init_settings();

    //init param
    init_ctrl_param();
    init_pid();
    
    return E_SUCCESS;
}

int32_t FC_I_V1_Conf::read_settings()
{
    uint32_t bw = 0;
     FRESULT result;
    
     /* 挂载文件系统 */
     result = f_mount(&m_fs, "0:", 1);         /* Mount a logical drive */
     if (result != FR_OK)
     {
         /* 如果挂载不成功，进行格式化 */
         result = f_mkfs("0:", 0, 0);
         if (result != FR_OK)
         {
              return -1;     //flash有问题，无法格式化
         }
         else
         {
             /* 重新进行挂载 */
             result = f_mount(&m_fs, "0:", 0);         /* Mount a logical drive */
            
            if (result != FR_OK)
            {
                     /* 卸载文件系统 */
               f_mount(NULL, "0:", 0);
                  return -2 ;
             }
         }
     }
    
     /* 打开根文件夹 */
     result = f_opendir(&m_dir, "/"); 
     if (result != FR_OK)
     {
         /* 卸载文件系统 */
       f_mount(NULL, "0:", 0);
         return -3;
     }
    
     /* 打开文件 */
     result = f_open(&m_file, SENSOR_CONF_FILE, FA_OPEN_EXISTING | FA_READ);
     if (result !=  FR_OK)
     {
       /* 卸载文件系统 */
       f_mount(NULL, "0:", 0);
        /* 文件不存在 */
         return -4;
     }
    
     /* 读取sensor配置文件 */
     result = f_read(&m_file, &sensor_setup.raw_data, sizeof(sensor_setup), &bw);
     if (bw > 0)
     {
          /* 关闭文件*/
          f_close(&m_file);
          
             /* 打开文件 */
          result = f_open(&m_file, PID_CONF_FILE, FA_OPEN_EXISTING | FA_READ);
           if (result !=  FR_OK)
          {
              /* 卸载文件系统 */
              f_mount(NULL, "0:", 0);
              return -4;
          }
      
         /* 读取PID配置文件 */
        result = f_read(&m_file, &pid_setup.raw_data, sizeof(pid_setup), &bw);
        if(bw > 0)
        {
            /* 关闭文件*/
            f_close(&m_file);
            
             /* 卸载文件系统 */
            f_mount(NULL, "0:", 0);
            return E_SUCCESS;
        }
        else
        {
            /* 关闭文件*/
            f_close(&m_file);
            
             /* 卸载文件系统 */
            f_mount(NULL, "0:", 0);
            return -4;
         }
     }
     else
     {
          /* 关闭文件*/
          f_close(&m_file);
          
             /* 卸载文件系统 */
          f_mount(NULL, "0:", 0);
          
          return -5;
     }    
}

int32_t FC_I_V1_Conf::reset_settings()
{
    /* 如果挂载不成功，进行格式化 */
    f_mkfs("0:", 1, 0);
    
    /* PID 默认值 */
    pid_setup.groups.ctrl1.pitch.kp = 0.6;
    pid_setup.groups.ctrl1.roll.kp  = 0.6;    
    pid_setup.groups.ctrl1.yaw.kp   = 1.0;    
    
    pid_setup.groups.ctrl1.pitch.ki = 0.1;
    pid_setup.groups.ctrl1.roll.ki  = 0.1;    
    pid_setup.groups.ctrl1.yaw.ki   = 0.1;    
    
    
    pid_setup.groups.ctrl1.pitch.kd = 2.2;
    pid_setup.groups.ctrl1.roll.kd  = 2.2;    
    pid_setup.groups.ctrl1.yaw.kd   = 2.0;    
    
    pid_setup.groups.ctrl1.pitch.kdamp = 1;
    pid_setup.groups.ctrl1.roll.kdamp  = 1;    
    pid_setup.groups.ctrl1.yaw.kdamp   = 1;

    pid_setup.groups.ctrl2.pitch.kp = 0.5;
    pid_setup.groups.ctrl2.roll.kp  = 0.5;
    pid_setup.groups.ctrl2.yaw.kp   = 0.8;    
    
    pid_setup.groups.ctrl2.pitch.ki = 0.05;
    pid_setup.groups.ctrl2.roll.ki  = 0.05;    
    pid_setup.groups.ctrl2.yaw.ki   = 0.05;    
    
    pid_setup.groups.ctrl2.pitch.kd = 0.3;
    pid_setup.groups.ctrl2.roll.kd  = 0.3;
    pid_setup.groups.ctrl2.yaw.kd   = 0.1;
    
    pid_setup.groups.ctrl3.kp = 1.0f;
    pid_setup.groups.ctrl3.ki = 1.0f;
    pid_setup.groups.ctrl3.kd = 1.0f;
    
    pid_setup.groups.ctrl4.kp = 1.0f;
    pid_setup.groups.ctrl4.ki = 1.0f;
    pid_setup.groups.ctrl4.kd = 1.0;
    
    pid_setup.groups.hc_sp.kp = 1.0f;
    pid_setup.groups.hc_sp.ki = 1.0f;
    pid_setup.groups.hc_sp.kd = 1.0f;
    
    pid_setup.groups.hc_height.kp = 1.0f;
    pid_setup.groups.hc_height.ki = 1.0f;
    pid_setup.groups.hc_height.kd = 1.0f;    
    
    write_settings();
    
    init_settings();    
    init_ctrl_param();
    init_pid(); 
		
		return E_SUCCESS;
}

int32_t FC_I_V1_Conf::write_settings()
{
    uint32_t bw = 0;
    FRESULT result;

     /* 挂载文件系统 */
    result = f_mount(&m_fs, "0:", 0);            /* Mount a logical drive */
    if (result != FR_OK)
    {
        /* 如果挂载不成功，进行格式化 */
        result = f_mkfs("0:", 0, 0);
        if (result != FR_OK)
        {
             return -1;     //flash有问题，无法格式化
        }
        else
        {
            /* 重新进行挂载 */
            result = f_mount(&m_fs, "0:", 0);            /* Mount a logical drive */
            if (result != FR_OK)
            {
                 /* 卸载文件系统 */
                f_mount(NULL, "0:", 0);
                return -2 ;
            }
        }
    }

    /* 打开根文件夹 */
    result = f_opendir(&m_dir, "/"); 
    if (result != FR_OK)
    {
        /* 卸载文件系统 */
        f_mount(NULL, "0:", 0);
        return -3;
    }

    /* 打开文件 */
    result = f_open(&m_file, SENSOR_CONF_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (result !=  FR_OK)
    {
        /* 卸载文件系统 */
        f_mount(NULL, "0:", 0);
        return -4;
    }

    /* 写入sensor配置文件 */
    result = f_write(&m_file, &sensor_setup.raw_data, sizeof(sensor_setup), &bw);
    if (result == FR_OK)
    {
        /* 关闭文件*/
        f_close(&m_file);
        
        /* 打开文件 */
        result = f_open(&m_file, PID_CONF_FILE, FA_CREATE_ALWAYS | FA_WRITE);
        if (result !=  FR_OK)
         {
            /* 卸载文件系统 */
            f_mount(NULL, "0:", 0);
            return -4;
         }
         
        /* 写入PID配置文件 */
        result = f_write(&m_file, &pid_setup.raw_data, sizeof(pid_setup), &bw);
        if(result == FR_OK)
        {
             /* 关闭文件*/
            f_close(&m_file);
            
             /* 卸载文件系统 */
            f_mount(NULL, "0:", 0);
            return E_SUCCESS;
        }
        else
        {
            /* 关闭文件*/
            f_close(&m_file);
            
             /* 卸载文件系统 */
            f_mount(NULL, "0:", 0);
            return -4;
        }
    }
    else
    {
      /* 关闭文件*/
      f_close(&m_file);
      
        /* 卸载文件系统 */
      f_mount(NULL, "0:", 0);
      return -5;
    }    
}

int32_t FC_I_V1_Conf::init_settings()
{
    memcpy(&(MPU6050_DEVICE->get_mpu6050_struct()->Acc_Offset), &sensor_setup.Offset.Accel, sizeof(xyz_f_t));
    memcpy(&(MPU6050_DEVICE->get_mpu6050_struct()->Gyro_Offset), &sensor_setup.Offset.Gyro, sizeof(xyz_f_t));
    //!!!memcpy(&ak8975.Mag_Offset,&sensor_setup.Offset.Mag,sizeof(xyz_f_t));
    memcpy(&(MPU6050_DEVICE->get_mpu6050_struct()->vec_3d_cali), &sensor_setup.Offset.vec_3d_cali, sizeof(xyz_f_t));
    
    MPU6050_DEVICE->get_mpu6050_struct()->Acc_Temprea_Offset = sensor_setup.Offset.Acc_Temperature;
    MPU6050_DEVICE->get_mpu6050_struct()->Gyro_Temprea_Offset = sensor_setup.Offset.Gyro_Temperature;
    
    /*!!!此处临时注销，写控制的时候再补齐
    memcpy(&ctrl_1.PID[PIDROLL], &pid_setup.groups.ctrl1.roll, sizeof(pid_t));
    memcpy(&ctrl_1.PID[PIDPITCH], &pid_setup.groups.ctrl1.pitch, sizeof(pid_t));
    memcpy(&ctrl_1.PID[PIDYAW], &pid_setup.groups.ctrl1.yaw, sizeof(pid_t));    
    
    memcpy(&ctrl_2.PID[PIDROLL], &pid_setup.groups.ctrl2.roll, sizeof(pid_t));
    memcpy(&ctrl_2.PID[PIDPITCH], &pid_setup.groups.ctrl2.pitch, sizeof(pid_t));
    memcpy(&ctrl_2.PID[PIDYAW], &pid_setup.groups.ctrl2.yaw, sizeof(pid_t));*/
	
    return E_SUCCESS;
}

int32_t FC_I_V1_Conf::init_pid()
{
    /*!!!
    h_acc_arg.kp = 0.01f ;                //比例系数
    h_acc_arg.ki = 0.02f  *pid_setup.groups.hc_sp.kp;                //积分系数
    h_acc_arg.kd = 0;                    //微积分系数
    h_acc_arg.k_pre_d = 0 ;    
    h_acc_arg.inc_hz = 0;
    h_acc_arg.k_inc_d_norm = 0.0f;
    h_acc_arg.k_ff = 0.05f;

    h_speed_arg.kp = 1.5f *pid_setup.groups.hc_sp.kp;                //比例系数
    h_speed_arg.ki = 0.0f *pid_setup.groups.hc_sp.ki;                //积分系数
    h_speed_arg.kd = 0.0f;                //微积分系数
    h_speed_arg.k_pre_d = 0.10f *pid_setup.groups.hc_sp.kd;
    h_speed_arg.inc_hz = 20;
    h_speed_arg.k_inc_d_norm = 0.8f;
    h_speed_arg.k_ff = 0.5f;    
    
    h_height_arg.kp = 1.5f *pid_setup.groups.hc_height.kp;            //比例系数
    h_height_arg.ki = 0.0f *pid_setup.groups.hc_height.ki;            //积分系数
    h_height_arg.kd = 0.05f *pid_setup.groups.hc_height.kd;           //微积分系数
    h_height_arg.k_pre_d = 0.01f ;
    h_height_arg.inc_hz = 20;
    h_height_arg.k_inc_d_norm = 0.5f;
    h_height_arg.k_ff = 0;
    */
    return E_SUCCESS;
}

int32_t FC_I_V1_Conf::init_ctrl_param()
{
    /*!!!此处临时注销, 写控制的时候补齐
    ctrl_1.PID[PIDROLL].kdamp  = 1;
    ctrl_1.PID[PIDPITCH].kdamp = 1;
    ctrl_1.PID[PIDYAW].kdamp  = 1;

    //外  0<fb<1
    ctrl_1.FB = 0.20;*/
    return E_SUCCESS;
}

void *FC_I_V1_Conf::get(uint32_t type)
{
    void * conf = NULL;
    
    switch(type)
    {
        case SENSOR_SETUP_CONF:
            conf = &m_sensor_conf;
            break;

        case PID_SETUP_CONF:
            conf = &m_pid_conf;
            break;
        
        default:
            break;
    }

    return conf;
}

int32_t FC_I_V1_Conf::save_mag_offset(xyz_f_t *offset)
{
    return E_SUCCESS;
}

int32_t FC_I_V1_Conf::save_accel_offset(xyz_f_t *offset);
{
    return E_SUCCESS;
}

int32_t FC_I_V1_Conf::save_gyro_offset(xyz_f_t *offset)
{
    return E_SUCCESS;
}




