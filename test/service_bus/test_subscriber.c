#include "test_service_bus.h"
#include "rtl.h"
#include "sem.h"
#include "common.h"
#include "topic_version.h"


//测试poll topic和从总线拷贝主题数据
__task void test_service_bus_subscriber()
{
    //subscribe
    version_t  version_subscriber;
    version_subscriber.version_tick = 0;
    //subscribe topic
    SUBSCRIBER_HANDLE handle = subscribe_topic(TOPIC_ID(version));
    
    while(true)
    {
        //poll topic and wait notification    
        int32_t ret = poll_topic(handle, 1000);
        if (SEM_OK == ret)
        {
            //copy topic data
            copy_topic_data(TOPIC_ID(version), handle, &version_subscriber);

            //display
        }
    }
}

//测试多线程从总线拷贝主题数据
__task void test_service_bus_subscribers_1()
{
    //subscribe
    version_t  version_subscriber_1;
    version_subscriber_1.version_tick = 0;
    //subscribe topic    
    SUBSCRIBER_HANDLE handle = subscribe_topic(TOPIC_ID(version));
    
    while(true)
    {
        //poll topic and wait notification    
        int32_t ret = poll_topic(handle, 1000);
        if (SEM_OK == ret)
        {
            //copy topic data        
            copy_topic_data(TOPIC_ID(version), handle, &version_subscriber_1);

            //display
        }
    }
}


//测试多线程从总线拷贝主题数据
__task void test_service_bus_subscribers_2()
{
    //subscribe
    version_t  version_subscriber_2;
    version_subscriber_2.version_tick = 0;
    //subscribe topic    
    SUBSCRIBER_HANDLE handle = subscribe_topic(TOPIC_ID(version));

    bool updated = false;    
    while(true)
    {
        //check topic data updated
        int32_t check_result = check_topic(handle, updated);
        if (updated)
        {
            //copy topic data
            copy_topic_data(TOPIC_ID(version), handle, &version_subscriber_2);

            //display
        }
    }
}

//测试多线程从总线检查主题存在、订阅主题、检查主题是否更新、拷贝主题数据
__task void test_service_bus_subscribers_3()
{
    version_t version;
    SUBSCRIBER_HANDLE handle = NULL;

    //check topic exists
    bool existed = exists_topic(TOPIC_ID(version));
    if (existed)
    {   
        //subscribe message topic
        handle = subscribe_topic(TOPIC_ID(version));
        
        subscriber_meta_data meta;
        meta.max_mail_count_in_box = 10;
        set_subscriber(handle, &meta);
    }
    else
    {
        return;
    }

    bool updated = false;
    while(true)
    {
        //check topic data updated
        int32_t check_result = check_topic(handle, updated);
        if (updated)
        {
            //copy message topic data
            copy_topic_data(TOPIC_ID(version), handle, &version);
        }
    }
}


