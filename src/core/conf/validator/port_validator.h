/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºip_validator.h
* description    £ºip validator for config parameter
* date                  : 2018.03.21
* author            £ºBruce Feng
**********************************************************************************/

#pragma once


#include "conf_validator.h"


namespace matrix
{
    namespace core
    {

        class port_validator : public conf_validator
        {
        public:

            bool validate(const variable_value &val)
            {
                try
                {
                    unsigned long port = val.as<unsigned long>();
                    return (0 == port || port > 65535) ? false : true;
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