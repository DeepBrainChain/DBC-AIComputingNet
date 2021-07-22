#pragma once

#include "service_module/service_module.h"
#include "container_resource_mng.h"

#define AI_PRUNE_CONTAINER_TIMER                                      "prune_task_container"
#define AI_PRUNE_CONTAINER_TIMER_INTERVAL                             (10*60*1000)                                                 //10min timer

class common_service : public service_module
{
public:
    common_service();

    virtual ~common_service() = default;

    virtual std::string module_name() const { return common_service_name; }

protected:
    virtual void init_timer();

    virtual int32_t service_init(bpo::variables_map &options);

    virtual int32_t service_exit();

    int32_t on_timer_prune_container(std::shared_ptr<core_timer> timer);

private:
    uint32_t m_prune_task_timer_id = INVALID_TIMER_ID;
    container_resource_mng m_container_resouce_mng;
};
