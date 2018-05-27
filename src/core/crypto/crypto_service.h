/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         :   crypto_service.h
* description    :   crypto service for initialization
* date                  :   2018.04.22
* author             :   Bruce Feng
**********************************************************************************/
#pragma once


#include <mutex>
#include "module.h"


namespace matrix
{
    namespace core
    {

        class crypto_service : public module
        {
        public:

            crypto_service() = default;

            ~crypto_service() = default;

            virtual std::string module_name() const { return "crypto service"; }

            virtual int32_t init(bpo::variables_map &options);

            virtual int32_t exit();
        };

    }

}