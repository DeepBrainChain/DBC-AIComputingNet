/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
 * file name        :   main.cpp
 * description    :   main function
 * date                  : 2017.01.23
 * author            :   Bruce Feng
**********************************************************************************/

#include <functional>
#include <chrono>
#include "log.h"
#include "start_up.h"
#include "dbc_server_initiator.h"
#include "server_initiator_factory.h"
#include "service/ai/deep_brain_chain/ai_training/db/ai_db_types.h"
#if defined(WIN32) || defined(__linux__) || defined(MAC_OSX)

using namespace std::chrono;
using namespace ai::dbc;
using namespace matrix::core;


high_resolution_clock::time_point server_start_time;
std::map< std::string, std::shared_ptr<ai::dbc::ai_training_task> > m_running_tasks;
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

int main(int argc, char* argv[])
{
    int32_t ret = pre_main_task();
    if (ret != E_SUCCESS)
    {
        return ret;
    }

    return main_task(argc, argv);
}

#else       //not support yet

#endif

