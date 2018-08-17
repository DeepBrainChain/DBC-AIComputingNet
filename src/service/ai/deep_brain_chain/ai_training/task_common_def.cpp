/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   task_common_def.h
* description    :   task common definition
* date                  :   2018.01.28
* author            :   Bruce Feng
**********************************************************************************/

#include "task_common_def.h"
#include <boost/xpressive/xpressive_dynamic.hpp>  

namespace ai
{
    namespace dbc
    {
        using namespace boost::xpressive;
        std::string to_training_task_status_string(int8_t status)
        {
            switch (status)
            {
            case task_unknown:
                return "task_unknown";
            case task_queueing:
                return "task_queueing";
            case task_running:
                return "task_running";
            case task_stopped:
                return "task_stopped";
            case task_succefully_closed:
                return "task_succefully_closed";
            case task_abnormally_closed:
                return "task_abnormally_closed";
            case task_overdue_closed:
                return "task_overdue_closed";
            case task_pulling_image:
                return "task_pulling_image";
            case task_noimage_closed:
                return "task_noimage_closed";
            case task_nospace_closed:
                return "task_nospace_closed";
            default:
                return DEFAULT_STRING;
            }
        }
        std::string engine_reg;
        bool check_task_engine(std::string engine)
        {
            if (engine_reg.empty())
            {
                return engine.length()!=0;
            }
            try 
            {
                cregex reg = cregex::compile(engine_reg);
                return regex_match(engine.c_str(), reg);
            }
            catch (...)
            {
                return false;
            }
            
            return false;
        }

        void set_task_engine(std::string engine)
        {
            engine_reg = engine;
        }

    }

}