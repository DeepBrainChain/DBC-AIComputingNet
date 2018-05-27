/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   dbc_conf_manager.cpp
* description    :   dbc config manager
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#include "dbc_conf_manager.h"
#include <memory>

namespace ai
{
    namespace dbc
    {
        std::unique_ptr<dbc_conf_manager> g_conf_manager(new dbc_conf_manager);

        int32_t dbc_conf_manager::init(bpo::variables_map &options)
        {
            //left to later

            return E_SUCCESS;
        }

    }

}