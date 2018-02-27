#include "FC_I_V1_service_node_manager.h"
#include "mems_service.h"
#include "barometer_service.h"
#include "magnetometer_service.h"
#include "ultrasonic_service.h"


volatile U64 stk_mems_service[512/8];
volatile U64 stk_mag_service[512/8];
volatile U64 stk_baro_service[512/8];
volatile U64 stk_ultrasonic_service[512/8];


int32_t FC_I_V1_ServiceNodeManager::create_service_nodes()
{
    //mems service
    create_mems_service();

    //mag service
    create_mag_service();

    //baro service
    create_baro_service();

    //ultrasonic service
    create_ultrasonic_service();
	
    return E_SUCCESS;
}

int32_t FC_I_V1_ServiceNodeManager::create_mems_service()
{
    service_node_meta_data meta;

    meta.task_priority = MEMS_SERVCE_NODE_PRIORITY;
    meta.stack = (void *)&stk_mems_service;
    meta.stack_size = sizeof(stk_mems_service);
    meta.max_message_count = DEFAULT_MAX_MESSAGE_COUNT;

    //mems service
    ServiceNode *node = new MemsService(&meta);
    node->init();
    node->start();

    m_service_node_list.add_tail(node);
    return E_SUCCESS;
}

int32_t FC_I_V1_ServiceNodeManager::create_baro_service()
{
    service_node_meta_data meta;

    meta.task_priority = BARO_SERVCE_NODE_PRIORITY;
    meta.stack = (void *)&stk_baro_service;
    meta.stack_size = sizeof(stk_baro_service);
    meta.max_message_count = DEFAULT_MAX_MESSAGE_COUNT;

    //mems service
    ServiceNode *node = new BarometerService(&meta);
    node->init();
    node->start();

    m_service_node_list.add_tail(node);
    return E_SUCCESS;
}

int32_t FC_I_V1_ServiceNodeManager::create_mag_service()
{
    service_node_meta_data meta;

    meta.task_priority = MAG_SERVCE_NODE_PRIORITY;
    meta.stack = (void *)&stk_mag_service;
    meta.stack_size = sizeof(stk_mag_service);
    meta.max_message_count = DEFAULT_MAX_MESSAGE_COUNT;

    //mems service
    ServiceNode *node = new MagnetometerService(&meta);
    node->init();
    node->start();

    m_service_node_list.add_tail(node);
    return E_SUCCESS;
}

int32_t FC_I_V1_ServiceNodeManager::create_ultrasonic_service()
{
    service_node_meta_data meta;

    meta.task_priority = ULTRASONIC_SERVCE_NODE_PRIORITY;
    meta.stack = (void *)&stk_ultrasonic_service;
    meta.stack_size = sizeof(stk_ultrasonic_service);
    meta.max_message_count = DEFAULT_MAX_MESSAGE_COUNT;

    //mems service
    ServiceNode *node = new UltrasonicService(&meta);
    node->init();
    node->start();

    m_service_node_list.add_tail(node);
    return E_SUCCESS;
}


