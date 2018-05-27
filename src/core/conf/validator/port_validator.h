/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ip_validator.h
* description    :   ip validator for config parameter
* date                  :   2018.03.21
* author            :   Bruce Feng
**********************************************************************************/

#pragma once


#include "conf_validator.h"
#include <boost/xpressive/xpressive_dynamic.hpp>  
using namespace boost::xpressive;
namespace matrix
{
    namespace core
    {
        
        
        class port_validator : public conf_validator
        {
        //modify by regulus:fix can't validate error port ("-4294967295"). use reg instead
        private:
            cregex m_reg_port = cregex::compile("(^[1-9]\\d{0,3}$)|(^[1-5]\\d{4}$)|(^6[0-4]\\d{3}$)|(^65[0-4]\\d{2}$)|(^655[0-2]\\d$)|(^6553[0-5]$)");
        public:
            bool validate(const variable_value &val)
            {                
                try
                {
                    std::string port = val.as<std::string>(); 
                    return regex_match(port.c_str(), m_reg_port);
                }
                catch (const boost::exception & e)
                {
                    LOG_ERROR << "port_validator validate exception: " << diagnostic_information(e);
                    return false;
                }
            }

        };

         

    }

}