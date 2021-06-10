/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : container_resource_mng.h
* description       : 
* date              : 2019/1/10
* author            : Regulus
**********************************************************************************/
#pragma once

#include <iostream>
#include<windows.h>
#include<string>
#include <boost/winapi/dll.hpp>
#include <boost/winapi/basic_types.hpp>
#ifdef _WIN64
namespace ai
{
    namespace dbc
    {
        class ansi_win_conv
        {
        public:
            ansi_win_conv()=default;
            ~ansi_win_conv();
            int32_t load_dll();
            void release_dll();
        private:
            HMODULE m_ansi_mod;
        };
    }
}
#endif