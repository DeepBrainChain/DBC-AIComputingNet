#include "test_service_bus.h"
#include "rtl.h"
#include "sem.h"



//²âÊÔº¯Êı
__task void test_service_bus()
{
    os_tsk_create(test_service_bus_publisher, 0);
    os_tsk_create(test_service_bus_publisher_1, 0);
    os_tsk_create(test_service_bus_publisher_2, 0);

    os_tsk_create(test_service_bus_subscriber, 0);
    os_tsk_create(test_service_bus_subscribers_1, 0);
    os_tsk_create(test_service_bus_subscribers_2, 0);
    os_tsk_create(test_service_bus_subscribers_3, 0);
}

