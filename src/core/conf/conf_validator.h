/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   conf_validator.h
* description    :   config validator for config parameter
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include <boost/program_options.hpp>


using namespace boost::program_options;


namespace matrix
{
    namespace core
    {
        class conf_validator
        {
        public:

            virtual bool validate(const variable_value &val) = 0;

        };

    }

}

