#include "FC_I_V1_Alarm.h"



void FC_I_V1_Alarm::alarm_debug(uint32_t type, const char * psz)
{
    
}

void FC_I_V1_Alarm::alarm_info(uint32_t type, const char * psz)
{
    switch (type)
    {
    case ALARM_INFO_ALIVE:
        alarm_info_alive();
        break;
        
    default:
        break;
    }    
}

void FC_I_V1_Alarm::alarm_warning(uint32_t type, const char * psz)
{
    
}

void FC_I_V1_Alarm::alarm_critical(uint32_t type, const char * psz)
{
    switch (type)
    {
    case ALARM_CRITICAL_MPU_ERR:
        alarm_critical_mpu_err();
        break;

    case ALARM_CRITICAL_MAG_ERR:
        alarm_critical_mag_err();
        break;
        
    case ALARM_CRITICAL_BARO_ERR:
        alarm_critical_ms5611_err();
        break;
        
    default:
        break;
    }
}

void FC_I_V1_Alarm::alarm_info_alive()
{
    
}

void FC_I_V1_Alarm::alarm_critical_mpu_err()
{
    
}

void FC_I_V1_Alarm::alarm_critical_mag_err()
{
    
}

void FC_I_V1_Alarm::alarm_critical_ms5611_err()
{
    
}


