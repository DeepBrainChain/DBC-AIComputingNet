/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºdbc_conf_manager.h
* description    £ºdbc config manager
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once

#include "conf_manager.h"

namespace bpo = boost::program_options;

namespace ai
{
    namespace dbc
    {

        class dbc_conf_manager;
        extern std::unique_ptr<dbc_conf_manager> g_conf_manager;

        class dbc_conf_manager : public matrix::core::conf_manager
        {
        public:

            int32_t init(bpo::variables_map &options);

            std::string module_name() const { return "dbc_conf_manager"; }

        };

    }

}
