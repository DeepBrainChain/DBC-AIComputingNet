/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：server.cpp
* description    ：core is abstracted into server, server is responsible for init and exit
* date                  : 2017.01.23
* author            ：Bruce Feng
**********************************************************************************/
#include "server.h"
#include <memory>
#include <cassert>
#include <thread>
#include "common.h"
#include "server_initiator.h"


//extern matrix::core::server_initiator * create_initiator();

namespace matrix
{
    namespace core
    {

        std::unique_ptr<server> g_server(new server());

        int32_t server::init(int argc, char* argv[])
        {
            m_initiator = shared_ptr<server_initiator>(m_init_factory.create_initiator());
            return (m_init_result = m_initiator->init(argc, argv));
        }

        int32_t server::idle()
        {
            while (!m_exited)
            {
                std::this_thread::sleep_for(1s);

                //to do idle task.
                do_cycle_task();
            }

            return E_SUCCESS;
        }

        void server::do_cycle_task()
        {
            //LOG_DEBUG << "server is running and doing idle cycle task";
        }

        int32_t server::exit()
        {
            m_module_manager->stop();
            m_module_manager->exit();

            return E_SUCCESS;                           //release resources with smart pointer
        }

    }

}

