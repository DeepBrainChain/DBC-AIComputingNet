#include "barometer_service.h"
#include "service_version.h"


BarometerService::BarometerService(service_node_meta_data *meta) : ServiceNode(meta)
{

}

BarometerService::~BarometerService()
{

}

int32_t BarometerService::on_time_out(Timer *timer)
{
    uint32_t timer_name = timer->get_name();

    switch (timer_name)
    {
        case BARO_TIMER_NAME:
            on_read_baro();
            break;

        default:
            break;
    }

    return E_SUCCESS;
}
    
int32_t BarometerService::service_init()
{
    m_baro_timer_id = m_timer_manager->add_timer(BARO_TIMER_NAME, BARO_PERIOD_MS);
    return E_SUCCESS;
}

int32_t BarometerService::service_exit()
{
    m_timer_manager->remove_timer(m_baro_timer_id);
    return E_SUCCESS;
}

int32_t BarometerService::on_invoke(message_t *message)
{
    return E_SUCCESS;
}

int32_t BarometerService::on_read_baro()
{
    MS5611_DEVICE->update();
    return E_SUCCESS;
}


