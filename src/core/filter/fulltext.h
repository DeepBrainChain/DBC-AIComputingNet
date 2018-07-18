/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : fulltext.h
* description       : 
* date              : 2018/7/3
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <string>
#include <vector>

namespace matrix
{
    namespace core
    {
        class fulltext
        {
        public:
            static bool search(std::string text, std::vector<std::string> keywords);

        };
    }
}