#include "magnetometer_service.h"
#include "service_version.h"


MagnetometerService::MagnetometerService(service_node_meta_data *meta) : ServiceNode(meta)
{

}

MagnetometerService::~MagnetometerService()
{

}

int32_t MagnetometerService::on_time_out(Timer *timer)
{
    uint32_t timer_name = timer->get_name();

    switch (timer_name)
    {
        case MAG_TIMER_NAME:
            on_read_mag();
            break;

        default:
            break;
    }

    return E_SUCCESS;
}

int32_t MagnetometerService::service_init()
{
    m_mag_timer_id = m_timer_manager->add_timer(MAG_TIMER_NAME, MAG_PERIOD_MS);
    return E_SUCCESS;
}

int32_t MagnetometerService::service_exit()
{
    m_timer_manager->remove_timer(m_mag_timer_id);
    return E_SUCCESS;
}

int32_t MagnetometerService::on_invoke(message_t *message)
{
    return E_SUCCESS;
}

int32_t MagnetometerService::on_read_mag()
{
    AK8975_DEVICE->read_data();
    return E_SUCCESS;
}


