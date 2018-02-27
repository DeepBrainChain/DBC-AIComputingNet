#include "ultrasonic_service.h"
#include "service_version.h"


UltrasonicService::UltrasonicService(service_node_meta_data *meta) : ServiceNode(meta)
{

}

UltrasonicService::~UltrasonicService()
{

}

int32_t UltrasonicService::on_time_out(Timer *timer)
{
    uint32_t timer_name = timer->get_name();

    switch (timer_name)
    {
        case ULTRASONIC_TIMER_NAME:
            on_read_ultrasonic();
            break;

        default:
            break;
    }

    return E_SUCCESS;
}
    
int32_t UltrasonicService::service_init()
{
    m_ultrasonic_timer_id = m_timer_manager->add_timer(ULTRASONIC_TIMER_NAME, ULTRASONIC_PERIOD_MS);
    return E_SUCCESS;
}

int32_t UltrasonicService::service_exit()
{
    m_timer_manager->remove_timer(m_ultrasonic_timer_id);
    return E_SUCCESS;
}

int32_t UltrasonicService::on_invoke(message_t *message)
{
    return E_SUCCESS;
}

int32_t UltrasonicService::on_read_ultrasonic()
{
    ULTRASONIC_DEVICE->read();
    return E_SUCCESS;
}


