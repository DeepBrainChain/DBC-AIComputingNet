#include "common_service.h"
#include "timer/timer_def.h"
#include "service_proto_filter.h"
#include "service_module/service_name.h"

common_service::common_service()
    : m_prune_task_timer_id(INVALID_TIMER_ID)
{

}

void common_service::init_timer()
{
    m_timer_invokers[AI_PRUNE_CONTAINER_TIMER] = std::bind(&common_service::on_timer_prune_container, this, std::placeholders::_1);
    m_prune_task_timer_id = add_timer(AI_PRUNE_CONTAINER_TIMER, AI_PRUNE_CONTAINER_TIMER_INTERVAL, ULLONG_MAX, DEFAULT_STRING);
}

int32_t common_service::service_init(bpo::variables_map &options)
{
    if (options.count(SERVICE_NAME_AI_TRAINING))
    {
        //only ai service nodes are needed to prune container resource
        if (E_SUCCESS != m_container_resouce_mng.init())
        {
            return E_DEFAULT;
        }
    }
    else
    {
        if (m_prune_task_timer_id != INVALID_TIMER_ID)
        {
            remove_timer(m_prune_task_timer_id);
        }
    }
    
    return E_SUCCESS;
}

int32_t common_service::service_exit()
{
    if (m_prune_task_timer_id != INVALID_TIMER_ID)
    {
        remove_timer(m_prune_task_timer_id);
    }
    return E_SUCCESS;
}
int32_t common_service::on_timer_prune_container(std::shared_ptr<core_timer> timer)
{
    m_container_resouce_mng.exec_prune();
    return E_SUCCESS;
}
