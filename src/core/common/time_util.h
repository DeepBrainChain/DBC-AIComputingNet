/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name:        time_util.h
* description:      time util
* date:             2018.06.05
* author:           Allan
**********************************************************************************/

#pragma once

#include <string>
#include <iostream>
#include <time.h>


namespace matrix
{
    namespace core
    {
        class time_util
        {
        public:
            static std::string time_2_str(time_t t)
            {
                struct tm _tm;
#ifdef WIN32
                int err = 0;
                err = localtime_s(&_tm, &t);
                if (err != 0)
                {
                    //std::cout << "get local time err: " << err << std::endl;
                    return "";
                }
#else
                localtime_r(&t, &_tm);
#endif
                char buf[256];
                memset(buf, 0, sizeof(char) * 256);
                strftime(buf, sizeof(char) * 256, "%x %X", &_tm);

                return buf;
            }

            static std::string time_2_utc(time_t t)
            {
                std::string temp = std::asctime(std::gmtime(&t));
                
                return temp.erase(temp.find("\n"));
            }

        };
    }

}
