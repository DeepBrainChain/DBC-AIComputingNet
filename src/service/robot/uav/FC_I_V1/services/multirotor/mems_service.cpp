#include "mems_service.h"
#include "service_version.h"
#include "os_time.h"


MemsService::MemsService(service_node_meta_data *meta) : ServiceNode(meta), m_cur_read_us(0), m_old_read_us(0)
{
        
}

MemsService::~MemsService()
{

}

int32_t MemsService::service_init() 
{
    m_mems_timer_id = m_timer_manager->add_timer(MEMS_TIMER_NAME, MEMS_PERIOD_MS);
    return E_SUCCESS;
}

int32_t MemsService::service_exit()
{
    m_timer_manager->remove_timer(m_mems_timer_id);
    return E_SUCCESS;
}

int32_t MemsService::on_time_out(Timer *timer)
{
    uint32_t timer_name = timer->get_name();

    switch (timer_name)
    {
        case MEMS_TIMER_NAME:
            on_read_mems();
            break;

        default:
            break;
    }

    return E_SUCCESS;
}
    
int32_t MemsService::on_invoke(message_t *message)
{
    return E_SUCCESS;
}

int32_t MemsService::on_read_mems()
{
    MPU6050_DEVICE->read();

    cal_cycle();
    MPU6050_DEVICE->prepare_data(m_cycle);

    return E_SUCCESS;
}

float MemsService::cal_cycle()
{
    m_cur_read_us = OSTime::get_os_time_us();
    m_cycle = (m_cur_read_us - m_old_read_us) / 1000000.0f;
    m_old_read_us = m_cur_read_us;

    return m_cycle;
}




