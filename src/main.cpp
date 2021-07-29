#include <functional>
#include <chrono>
#include "log/log.h"
#include "server/start_up.h"
#include "server/dbc_server_initiator.h"
#include "server/server_initiator_factory.h"

high_resolution_clock::time_point server_start_time;
//std::map< std::string, std::shared_ptr<ai::dbc::ai_training_task> > m_running_tasks;
//define how to create initiator
server_initiator * create_initiator()
{
    return new dbc_server_initiator();
}

//prepare for main task
int pre_main_task()
{
    //start time point
    server_start_time = high_resolution_clock::now();

    //init log
    int32_t ret = log::init();
    if (ret != E_SUCCESS)
    {
        return ret;
    }

    //bind init creator
    LOG_INFO << "------dbc is starting------";
    server_initiator_factory::bind_creator(create_functor_type(create_initiator));

    return 0;
}

#include "data/resource/SystemResourceManager.h"
#include "data/task/TaskResourceManager.h"
//#define MYTEST

int main(int argc, char* argv[])
{
    SystemResourceMgr::instance().Init();

#ifdef MYTEST
    SystemResourceMgr::instance().print_os();
    SystemResourceMgr::instance().print_cpu();
    SystemResourceMgr::instance().print_gpu();
    SystemResourceMgr::instance().print_mem();
    SystemResourceMgr::instance().print_disk();

    std::vector<std::string> vec;
    vec.push_back("666txxDVSv39JqbfRkBN9oQcCzyTfLmJYodUs8jG1t89cqPd5zqupbT");
    TaskResourceManager mgr;
    mgr.init(vec);
    mgr.print_cpu("666txxDVSv39JqbfRkBN9oQcCzyTfLmJYodUs8jG1t89cqPd5zqupbT");
    mgr.print_mem("666txxDVSv39JqbfRkBN9oQcCzyTfLmJYodUs8jG1t89cqPd5zqupbT");
    mgr.print_gpu("666txxDVSv39JqbfRkBN9oQcCzyTfLmJYodUs8jG1t89cqPd5zqupbT");
    mgr.print_disk("666txxDVSv39JqbfRkBN9oQcCzyTfLmJYodUs8jG1t89cqPd5zqupbT");

    return 0;
#endif

    int32_t ret = pre_main_task();
    if (ret != E_SUCCESS)
    {
        return ret;
    }

    return main_task(argc, argv);
}
