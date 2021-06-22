/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : fulltext.cpp
* description       : 
* date              : 2018/7/3
* author            : Jimmy Kuang
**********************************************************************************/

#include "filter/fulltext.h"
#include <algorithm>

namespace matrix
{
    namespace core
    {
        bool fulltext::search(std::string text, std::vector<std::string> keywords)
        {
            transform(text.begin(), text.end(), text.begin(), ::tolower);

            for (std::string &k: keywords)
            {
                transform(k.begin(), k.end(), k.begin(), ::tolower);
                if (text.find(k) == std::string::npos)
                {
                    return false;
                }
            }

            return true;
        }
    }
}