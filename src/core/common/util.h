/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºutil.h
* description    £ºcommon util tool 
* date                  : 2018.03.21
* author            £ºBruce Feng
**********************************************************************************/

#pragma once

#include <string>
#include <vector>

namespace matrix
{
    namespace core
    {
        class string_util
        {
        public:

            static void split(const std::string & str, const std::string& delim, std::vector<std::string> &vec)
            {
                size_t pos = 0;
                size_t index = str.find_first_of(delim, pos);
                while (index != std::string::npos)
                {
                    //push back
                    vec.push_back(str.substr(pos, index - pos));

                    pos = index + 1;
                    index = str.find_first_of(delim, pos);
                }

                //index is npos, and left string to end
                std::string left = str.substr(pos, std::string::npos);
                if (left.size() > 0)
                {
                    vec.push_back(left);
                }
            }

            static void split(char *str, char delim, int &argc, char* argv[])
            {
                if (nullptr == str || nullptr == argv || argc <= 0)
                {
                    argc = 0;
                    return;
                }

                int i = 0;
                char *p = str;

                while (*p)
                {
                    if (delim == *p)
                    {
                        *p++ = '\0';
                        continue;
                    }

                    argv[i++] = p;
                    if (i >= argc)
                    {
                        return;         //argc is unchanged
                    }

                    while (*p && delim != *p)
                    {
                        p++;
                    }
                }

                argc = i;
            }

            static void trim(std::string & str)
            {
                if (str.empty())
                {
                    return;
                }

                //header
                if (str[0] == ' ')
                {
                    str.erase(0, str.find_first_not_of(" "));
                }

                //tail
                if (str[str.length() - 1] == ' ')
                {
                    str.erase(str.find_last_not_of(" ") + 1);
                }
            }

        };

    }


}