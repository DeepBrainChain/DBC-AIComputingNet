/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   service_context.h
* description    :   service context for service module
* date                  :   2018.05.06
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include <boost/program_options/variables_map.hpp>

using namespace boost::program_options;


namespace matrix
{
    namespace core
    {

        class service_context
        {
        public:

            variables_map & get_args() { return m_args; }

            void add(std::string arg_name, variable_value val) { m_args.insert({ arg_name, val }); }

        protected:

            variables_map m_args;           //put variable args into context
        };

    }

}
