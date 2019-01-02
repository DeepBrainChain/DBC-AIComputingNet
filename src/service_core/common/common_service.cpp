#include "common_service.h"
#include "timer_def.h"
#include "service_proto_filter.h"

using namespace matrix::service_core;
using namespace matrix::core;

common_service::common_service()
    : m_timer_id_filter_clean(INVALID_TIMER_ID)
{

}

void common_service::init_invoker()
{

}

void common_service::init_timer()
{
    m_timer_invokers[TIMER_NAME_FILTER_CLEAN] = std::bind(&common_service::on_timer_filter_clean, this, std::placeholders::_1);
    m_timer_id_filter_clean = add_timer(TIMER_NAME_FILTER_CLEAN, TIMER_INTERV_SEC_FILTER_CLEAN * 1000);
}

int32_t common_service::service_init(bpo::variables_map &options)
{
    return E_SUCCESS;
}

int32_t common_service::service_exit()
{
    if (m_timer_id_filter_clean != INVALID_TIMER_ID)
    {
        remove_timer(m_timer_id_filter_clean);
    }

    return E_SUCCESS;
}

int32_t common_service::on_timer_filter_clean(std::shared_ptr<core_timer> timer)
{
    //service_proto_filter::get_mutable_instance().regular_clean();

    return E_SUCCESS;
}
