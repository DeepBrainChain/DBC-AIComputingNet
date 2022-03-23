#ifndef DBC_PORT_VALIDATOR_H
#define DBC_PORT_VALIDATOR_H

#include "conf_validator.h"
#include <boost/xpressive/xpressive_dynamic.hpp>  

using namespace boost::xpressive;

class port_validator : public conf_validator
{
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
            return false;
        }
    }

};

#endif
