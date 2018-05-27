/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   channel_handler_contex.h
* description      :   channel handler context for parameter
* date             :   2018.01.20
* author           :   Bruce Feng
**********************************************************************************/
#pragma once

#include <boost/program_options/variables_map.hpp>

using namespace boost::program_options;

namespace matrix
{
    namespace core
    {

        class channel_handler_context
        {
        public:
            
            variables_map &get_args() { return m_args; }

        protected:

            variables_map m_args;           //put variable args into context
        };

    }

}

