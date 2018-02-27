/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        ：server_initiator_factory.cpp
* description    ：server initiator factory
* date                  : 2017.08.02
* author            ：Bruce Feng
**********************************************************************************/
#include "server_initiator_factory.h"
#include "server_initiator.h"


namespace matrix
{
    namespace core
    {
        create_functor_type server_initiator_factory::m_initiator_creator = std::function<server_initiator*()>(nullptr);

        server_initiator *server_initiator_factory::create_initiator()
        {
            return m_initiator_creator();           //bind and lazy execute to avoid service invasion
        }

    }

}


