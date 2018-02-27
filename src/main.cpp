/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
 * file name        ：main.cpp
 * description    ：main function
 * date                  : 2017.01.23
 * author            ：Bruce Feng
**********************************************************************************/

#include <functional>
#include "log.h"
#include "start_up.h"
#include "dbc_server_initiator.h"
#include "server_initiator_factory.h"


#ifdef __RTX
int main(void)
{
    os_sys_init_user(main_task, 1, (void *)&stk_main, sizeof(stk_main));
}

#elif defined(WIN32) || defined(__linux__) || defined(MAC_OSX)

using namespace ai::dbc;
using namespace matrix::core;

//define how to create initiator
server_initiator * create_initiator()
{                  
    return new dbc_server_initiator();
}

//prepare for main task
int pre_main_task()
{
    LOG_DEBUG << "------dbc is starting------";

    //init log
    log::init();

    //bind init creator
    server_initiator_factory::bind_creator(create_functor_type(create_initiator));
    return 0;
}

int main(int argc, char* argv[])
{
    pre_main_task();
    
    return main_task(argc, argv);
}

#else       //not support yet

#endif

