#include "test_service_manager.h"


volatile U64 stk_sensor[1024 / 8];
volatile U64 stk_attitude[1204 / 8];


__task void test_service_node()
{
    TestServiceNodeManager *service_manager = new TestServiceNodeManager();
    service_manager->init();

    while(true);    
}

