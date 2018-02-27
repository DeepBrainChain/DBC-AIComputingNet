#include "test_service_bus.h"
#include "rtl.h"
#include "sem.h"
#include "topic_version.h"


//测试发布数据
__task void test_service_bus_publisher()
{
    /*version_t version_publish;
    version_publish.version_tick = 0;        

    //register topic
    PUBLISHER_HANDLE handle = register_topic(TOPIC_ID(version));
    
    while(true)
    {
        //publish topic data
        publish_topic_data(handle, &version_publish);
        version_publish.version_tick++;
    }*/
}

//测试发布数据
__task void test_service_bus_publisher_1()
{
    /*version_t version_publish_1;
    version_publish_1.version_tick = 0;        

    //register topic
    PUBLISHER_HANDLE handle = register_topic(TOPIC_ID(version));
    
    while(true)
    {
        //publish topic data
        publish_topic_data(handle, &version_publish_1);
        version_publish_1.version_tick++;
    }*/
}

//测试发布数据
__task void test_service_bus_publisher_2()
{
    //declare message
    /*version_t version_publish_2;
    version_publish_2.version_tick = 0;

    //register message topic
    PUBLISHER_HANDLE handle = register_topic(TOPIC_ID(version));
    
    while(1)
    {
        //publish topic data
        publish_topic_data(handle, &version_publish_2);

        //update message
        version_publish_2.version_tick++;
    }*/
}


