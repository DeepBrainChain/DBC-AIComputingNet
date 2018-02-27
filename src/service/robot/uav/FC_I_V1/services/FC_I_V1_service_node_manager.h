/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : FC_I_V1_service_node_manager.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年11月26日
  最近修改   :
  功能描述   : 本文件负责FC_I_V1飞控的service node管理
  函数列表   :
  修改历史   :
  1.日    期   : 2017年11月26日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _FC_I_V1_SERVICE_NODE_MANAGER_H_
#define _FC_I_V1_SERVICE_NODE_MANAGER_H_


#include "service_node_manager.h"


#define MEMS_SERVCE_NODE_PRIORITY                           254
#define MAG_SERVCE_NODE_PRIORITY                             253
#define BARO_SERVCE_NODE_PRIORITY                            252
#define ULTRASONIC_SERVCE_NODE_PRIORITY             251





class FC_I_V1_ServiceNodeManager : public ServiceNodeManager
{
public:

    FC_I_V1_ServiceNodeManager() {}

    virtual ~FC_I_V1_ServiceNodeManager() {}

protected:

    virtual int32_t create_service_nodes();

    //virtual int32_t destroy_service_nodes();


protected:

    virtual int32_t create_mems_service();

    virtual int32_t create_baro_service();

    virtual int32_t create_mag_service();

    virtual int32_t create_ultrasonic_service();
    

};

#endif


