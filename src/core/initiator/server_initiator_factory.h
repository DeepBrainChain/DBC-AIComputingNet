/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   server_initiator_factory.hpp
* description    :   server initiator factory
* date                  : 2017.08.02
* author            :   Bruce Feng
**********************************************************************************/
#pragma once


#include <functional>
#include "server_initiator.h"


namespace matrix
{
    namespace core
    {

        using create_functor_type = std::function<server_initiator*()>;

        class server_initiator_factory
        {
        public:

            server_initiator_factory() {}

            virtual ~server_initiator_factory() {}

            //lazy create
            static void bind_creator(create_functor_type initiator_creator) { m_initiator_creator = initiator_creator; }

            virtual server_initiator * create_initiator();

        protected:

            static create_functor_type m_initiator_creator;
        };

    }

}




