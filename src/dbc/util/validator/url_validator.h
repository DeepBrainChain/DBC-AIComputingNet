#ifndef DBC_URL_VALIDATOR_H
#define DBC_URL_VALIDATOR_H

#include "common/common.h"
#include "conf_validator.h"
#include "network/http_client.h"

class url_validator : public conf_validator
{
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
            return false;
        }
    }
};

#endif
