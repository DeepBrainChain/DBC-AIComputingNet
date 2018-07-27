/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   ip_validator.h
* description    :   ip validator for config parameter
* date                  :   2018.07.29
* author            :   Regulus
**********************************************************************************/
#pragma once


#include "util.h"
#include "common.h"
#include "conf_validator.h"
#include "http_client.h"
using namespace boost::xpressive;

namespace matrix
{
    namespace core
    {

        class url_validator : public conf_validator
        {
        private:
            
        public:
            url_validator() = default;
            bool validate(const variable_value &val)
            {
                try
                {
                    std::string url = val.as<std::string>();
                    raii_evhttp_uri http_url =  obtain_evhttp_uri_parse(url);
                    if (http_url.get())
                    {
                        return true;
                    }
                    return false;
                }
                catch (const std::exception & e)
                {
                    LOG_ERROR << "url_validator validate exception: " << e.what();
                    return false;
                }
            }
        };

    }

}
